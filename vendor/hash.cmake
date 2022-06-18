FetchContent_Declare(
    hash
    GIT_REPOSITORY "https://github.com/stbrumme/hash-library.git"
    GIT_TAG "d389d18112bcf7e4786ec5e8723f3658a7f433d7"
    SOURCE_DIR "vendor/hash"
)

FetchContent_MakeAvailable(hash)

include_directories(${hash_SOURCE_DIR})
