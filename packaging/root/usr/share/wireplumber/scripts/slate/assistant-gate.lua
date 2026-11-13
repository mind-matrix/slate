-- Slate: only the selected voice assistant may use the microphone and speaker.
--
-- Assistants connect to the audio server as their own users (app-<id>) through Slate's
-- assistant socket. The shell publishes two keys in the "default" metadata:
--   slate.assistant.users   "<uid>:<app id> ..." for every installed assistant
--   slate.assistant.active  the selected assistant's id, or "" for none
-- Clients of any other assistant can't see audio devices, and their streams are never linked:
-- no microphone and no speaker. The script publishes slate.assistant.mic with the id of the
-- assistant that has a capture stream open, which drives the shell's mic badge.

log = Log.open_topic ("s-slate-assistant")
lutils = require ("linking-utils")
cutils = require ("common-utils")

local KEY_USERS = "slate.assistant.users"
local KEY_ACTIVE = "slate.assistant.active"
local KEY_MIC = "slate.assistant.mic"

local state = { users = {}, active = "", mic = nil }

local function parse_users (value)
  local users = {}
  for uid, id in string.gmatch (value or "", "(%d+):([%w%-]+)") do
    users[uid] = id
  end
  return users
end

clients_om = ObjectManager { Interest { type = "client" } }

devices_om = ObjectManager {
  Interest { type = "node", Constraint { "media.class", "matches", "Audio/*" } }
}

captures_om = ObjectManager {
  Interest { type = "node", Constraint { "media.class", "=", "Stream/Input/Audio" } }
}

metadata_om = ObjectManager {
  Interest { type = "metadata", Constraint { "metadata.name", "=", "default" } }
}

-- The assistant a client belongs to, or nil for everything else.
local function client_app (client)
  local uid = client.properties["pipewire.sec.uid"]
  return uid and state.users[tostring (uid)] or nil
end

local function blocked (app)
  return app ~= nil and app ~= state.active
end

local function node_app (node)
  local client_id = node.properties["client.id"]
  if not client_id then
    return nil
  end
  local client = clients_om:lookup {
    Constraint { "bound-id", "=", tonumber (client_id), type = "gobject" }
  }
  return client and client_app (client) or nil
end

-- Hides the audio devices from a blocked assistant, and shows them again once it's selected.
local function update_client (client)
  local app = client_app (client)
  if app == nil then
    return
  end
  local perms = blocked (app) and "-" or "rx"
  for node in devices_om:iterate () do
    client:update_permissions { [node["bound-id"]] = perms }
  end
end

local function publish_mic ()
  local metadata = metadata_om:lookup ()
  if not metadata then
    return
  end
  local mic = ""
  for node in captures_om:iterate () do
    local app = node_app (node)
    if app and not blocked (app) then
      mic = app
    end
  end
  if mic ~= state.mic then
    state.mic = mic
    metadata:set (0, KEY_MIC, "Spa:String", mic)
  end
end

local function rescan ()
  local source = Plugin.find ("standard-event-source")
  if source then
    source:call ("schedule-rescan", "linking")
  end
end

local function apply ()
  for client in clients_om:iterate () do
    update_client (client)
  end
  rescan ()
  publish_mic ()
end

local function read_metadata (metadata)
  local users = metadata:find (0, KEY_USERS)
  local active = metadata:find (0, KEY_ACTIVE)
  state.users = parse_users (users)
  state.active = active or ""
  log:info (string.format ("active assistant '%s', users '%s'", state.active, users or ""))
  apply ()
end

-- Removes every link of a session item, as linking/rescan.lua does for removed items.
local function remove_links (si, om)
  for silink in om:iterate { type = "SiLink" } do
    local out_id = tonumber (silink.properties["out.item.id"])
    local in_id = tonumber (silink.properties["in.item.id"])
    if out_id == si.id or in_id == si.id then
      local in_flags = lutils:get_flags (in_id)
      local out_flags = lutils:get_flags (out_id)
      if out_id == si.id and in_flags.peer_id == out_id then
        in_flags.peer_id = nil
      elseif in_id == si.id and out_flags.peer_id == in_id then
        out_flags.peer_id = nil
      end
      if cutils.parseBool (silink.properties["is.role.policy.link"]) then
        lutils.clearPriorityMediaRoleLink (silink)
      end
      silink:remove ()
    end
  end
end

-- Runs before any target is picked: a blocked assistant's stream gets no target and loses
-- any link it had.
SimpleEventHook {
  name = "linking/slate-assistant-gate",
  before = {
    "linking/find-media-role-target",
    "linking/find-defined-target",
    "linking/find-filter-target",
    "linking/find-default-target",
    "linking/find-best-target",
    "linking/prepare-link",
  },
  interests = {
    EventInterest {
      Constraint { "event.type", "=", "select-target" },
    },
  },
  execute = function (event)
    local _, om, si = lutils:unwrap_select_target_event (event)
    local node = si:get_associated_proxy ("node")
    local app = node and node_app (node) or nil
    if blocked (app) then
      log:info (si, string.format ("blocking %s: '%s' is not the selected assistant",
          tostring (node.properties["node.name"]), app))
      remove_links (si, om)
      event:stop_processing ()
    end
  end
}:register ()

metadata_om:connect ("object-added", function (_, metadata)
  read_metadata (metadata)
  metadata:connect ("changed", function (m, subject, key)
    if subject == 0 and (key == KEY_USERS or key == KEY_ACTIVE) then
      read_metadata (m)
    end
  end)
end)

clients_om:connect ("object-added", function (_, client)
  update_client (client)
end)

devices_om:connect ("object-added", function ()
  for client in clients_om:iterate () do
    update_client (client)
  end
end)

captures_om:connect ("object-added", publish_mic)
captures_om:connect ("object-removed", publish_mic)

clients_om:activate ()
devices_om:activate ()
captures_om:activate ()
metadata_om:activate ()
