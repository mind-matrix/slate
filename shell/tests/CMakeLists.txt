foreach(name theme nav)
    add_executable(test_${name} test_${name}.c)
    target_link_libraries(test_${name} PRIVATE slateui)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()

add_executable(test_sysinfo test_sysinfo.c)
target_link_libraries(test_sysinfo PRIVATE slateservices)
add_test(NAME sysinfo COMMAND test_sysinfo)
