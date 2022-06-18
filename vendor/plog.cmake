find_path(PLOG_INCLUDE_DIR NAMES plog/Init.h)
message("PLOG include dir: ${PLOG_INCLUDE_DIR}")

if (${PLOG_INCLUDE_DIR} STREQUAL "PLOG_INCLUDE_DIR-NOTFOUND")
  FetchContent_Declare(
      plog
      GIT_REPOSITORY "https://github.com/SergiusTheBest/plog.git"
      GIT_TAG "9f25561cc81460c557d8a29af4fccd2fde0f31fc"
      SOURCE_DIR "vendor/plog"
  )

  FetchContent_MakeAvailable(plog)

  include_directories(${plog_SOURCE_DIR}/include)
endif ()
