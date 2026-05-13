# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles/classroom-vault_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/classroom-vault_autogen.dir/ParseCache.txt"
  "classroom-vault_autogen"
  )
endif()
