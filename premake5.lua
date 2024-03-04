
solution "next"
	platforms { "portable", "x86", "x64", "avx", "avx2" }
	configurations { "Debug", "Release", "MemoryCheck" }
	targetdir "bin/"
	rtti "Off"
	warnings "Extra"
	floatingpoint "Fast"
	flags { "FatalWarnings" }
	defines { "NEXT_DEVELOPMENT" }
	filter "configurations:Debug"
		symbols "On"
		defines { "_DEBUG", "NEXT_ENABLE_MEMORY_CHECKS=1", "NEXT_ASSERTS=1" }
	filter "configurations:Release"
		optimize "Speed"
		defines { "NDEBUG" }
		editandcontinue "Off"
	filter "system:windows"
		location ("visualstudio")
	filter "platforms:*x86"
		architecture "x86"
	filter "platforms:*x64 or *avx or *avx2"
		architecture "x86_64"

project "next"
	kind "StaticLib"
-- todo: sodium needs updating to support sha256
	links { "sodium" }
	files {
		"include/next.h",
		"include/next_*.h",
		"source/next.cpp",
		"source/next_*.cpp",
	}
	includedirs { "include" } --, "sodium" }
	filter "system:windows"
		linkoptions { "/ignore:4221" }
		disablewarnings { "4324" }
	filter "system:macosx"
		linkoptions { "-framework SystemConfiguration -framework CoreFoundation" }

--project "sodium"
--	kind "StaticLib"
--	includedirs { "sodium" }
--	files {
--		"sodium/**.c",
--		"sodium/**.h",
--	}
--  	filter { "system:not windows", "platforms:*x64 or *avx or *avx2" }
--		files {
--			"sodium/**.S"
--		}
--	filter "platforms:*x86"
--		architecture "x86"
--		defines { "NEXT_X86=1", "NEXT_CRYPTO_LOGS=1" }
--	filter "platforms:*x64"
--		architecture "x86_64"
--		defines { "NEXT_X64=1", "NEXT_CRYPTO_LOGS=1" }
--	filter "platforms:*avx"
--		architecture "x86_64"
--		vectorextensions "AVX"
--		defines { "NEXT_X64=1", "NEXT_AVX=1", "NEXT_CRYPTO_LOGS=1" }
--	filter "platforms:*avx2"
--		architecture "x86_64"
--		vectorextensions "AVX2"
--		defines { "NEXT_X64=1", "NEXT_AVX=1", "NEXT_AVX2=1", "NEXT_CRYPTO_LOGS=1" }
--	filter "system:windows"
--		disablewarnings { "4221", "4244", "4715", "4197", "4146", "4324", "4456", "4100", "4459", "4245" }
--		linkoptions { "/ignore:4221" }
--	filter { "action:gmake" }
  --	buildoptions { "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unknown-pragmas", "-Wno-unused-variable", "-Wno-type-limits" }

project "test"
	kind "ConsoleApp"
	links { "next", "sodium" }
	files { "test.cpp" }
	includedirs { "include" }
	filter "system:windows"
		disablewarnings { "4324" }
	filter "system:not windows"
		links { "pthread" }
	filter "system:macosx"
		linkoptions { "-framework SystemConfiguration -framework CoreFoundation" }

project "soak"
	kind "ConsoleApp"
	links { "next", "sodium" }
	files { "soak.cpp" }
	includedirs { "include" }
	filter "system:windows"
		disablewarnings { "4324" }
	filter "system:not windows"
		links { "pthread" }
	filter "system:macosx"
		linkoptions { "-framework SystemConfiguration -framework CoreFoundation" }

project "fuzz"
	kind "ConsoleApp"
	links { "next", "sodium" }
	files { "soak.cpp" }
	defines { "FUZZ_TEST=1" }
	includedirs { "include" }
	filter "system:windows"
		disablewarnings { "4324" }
	filter "system:not windows"
		links { "pthread" }
	filter "system:macosx"
		linkoptions { "-framework SystemConfiguration -framework CoreFoundation" }

project "simple_client"
	kind "ConsoleApp"
	links { "next", "sodium" }
	files { "examples/simple_client.cpp" }
	includedirs { "include" }
	filter "system:windows"
		disablewarnings { "4324" }
	filter "system:not windows"
		links { "pthread" }
	filter "system:macosx"
		linkoptions { "-framework SystemConfiguration -framework CoreFoundation" }

project "simple_server"
	kind "ConsoleApp"
	links { "next", "sodium" }
	files { "examples/simple_server.cpp" }
	includedirs { "include" }
	filter "system:windows"
		disablewarnings { "4324" }
	filter "system:not windows"
		links { "pthread" }
	filter "system:macosx"
		linkoptions { "-framework SystemConfiguration -framework CoreFoundation" }

project "upgraded_client"
	kind "ConsoleApp"
	links { "next", "sodium" }
	files { "examples/upgraded_client.cpp" }
	includedirs { "include" }
	filter "system:windows"
		disablewarnings { "4324" }
	filter "system:not windows"
		links { "pthread" }
	filter "system:macosx"
		linkoptions { "-framework SystemConfiguration -framework CoreFoundation" }

project "upgraded_server"
	kind "ConsoleApp"
	links { "next", "sodium" }
	files { "examples/upgraded_server.cpp" }
	includedirs { "include" }
	filter "system:windows"
		disablewarnings { "4324" }
	filter "system:not windows"
		links { "pthread" }
	filter "system:macosx"
		linkoptions { "-framework SystemConfiguration -framework CoreFoundation" }

project "complex_client"
	kind "ConsoleApp"
	links { "next", "sodium" }
	files { "examples/complex_client.cpp" }
	includedirs { "include" }
	filter "system:windows"
		disablewarnings { "4324" }
	filter "system:not windows"
		links { "pthread" }
	filter "system:macosx"
		linkoptions { "-framework SystemConfiguration -framework CoreFoundation" }

project "complex_server"
	kind "ConsoleApp"
	links { "next", "sodium" }
	files { "examples/complex_server.cpp" }
	includedirs { "include" }
	filter "system:windows"
		disablewarnings { "4324" }
	filter "system:not windows"
		links { "pthread" }
	filter "system:macosx"
		linkoptions { "-framework SystemConfiguration -framework CoreFoundation" }
