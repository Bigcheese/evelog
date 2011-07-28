add_executable(lbw-dump
  lbw-dump.cpp
  )

target_link_libraries(lbw-dump
  evelog
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${Boost_DATE_TIME_LIBRARY}
  ${BOOST_REGEX_LIBRARY}
  ${BOOST_FILESYSTEM_LIBRARY}
  )
