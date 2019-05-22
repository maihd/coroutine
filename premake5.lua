local ROOT_DIR = path.getabsolute(".")
local BUILD_DIR = path.join(ROOT_DIR, "Build")

workspace "Coroutine"
do
    language "C"
    location (BUILD_DIR)

    platforms { "x32", "x64", "Native" }
    
    configurations { "Debug", "Release" }

    filter {}
end

project "Coroutine.Test"
do 
    kind "ConsoleApp"

    files {
        path.join(ROOT_DIR, "Coroutine.h"),
        path.join(ROOT_DIR, "Coroutine_Test.c"),
        path.join(ROOT_DIR, "Coroutine_Windows.c"),
    }

    filter {}
end