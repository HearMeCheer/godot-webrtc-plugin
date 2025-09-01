#!python

import os, sys, platform, json, subprocess
import SCons

DEFAULT_MACOS_DEPLOYMENT_TARGET = '15.0'

def add_sources(sources, dirpath, extension):
    for f in os.listdir(dirpath):
        if f.endswith("." + extension):
            sources.append(dirpath + "/" + f)

def replace_flags(flags, replaces):
    for k, v in replaces.items():
        if k not in flags:
            continue
        if v is None:
            flags.remove(k)
        else:
            flags[flags.index(k)] = v

def validate_godotcpp_dir(key, val, env):
    normalized = val if os.path.isabs(val) else os.path.join(env.Dir("#").abspath, val)
    if not os.path.isdir(normalized):
        raise UserError("GDExtension directory ('%s') does not exist: %s" % (key, val))

def increament_build_version(version_file):
    exists = os.path.exists(version_file)
    if exists:
        with open(version_file, "r") as f:
            version = int(f.read().strip())
    else:
        version = 0
        # create file and write 0
        with open(version_file, "w") as f:
            f.write("0")
    version += 1

    with open(version_file, "w") as f:
        f.write(str(version))

    print(f"Build version: {version}")

env = Environment()
opts = Variables(["customs.py"], ARGUMENTS)
opts.Add(EnumVariable("godot_version", "The Godot target version", "4.1", ["3", "4.0", "4.1"]))
opts.Add(
    PathVariable(
        "godot_cpp",
        "Path to the directory containing Godot CPP folder",
        None,
        validate_godotcpp_dir,
    )
)
AddOption("--versioned", dest="versioned", action="store_true", default=False, help="Enable versioned build")
opts.Update(env)


build_folder = "build"

# Minimum target platform versions.
if "ios_min_version" not in ARGUMENTS:
    ARGUMENTS["ios_min_version"] = "14.0"
if "macos_deployment_target" not in ARGUMENTS:
    print('No macOS deployment target specified, defaulting to %s' % DEFAULT_MACOS_DEPLOYMENT_TARGET)
    ARGUMENTS["macos_deployment_target"] = DEFAULT_MACOS_DEPLOYMENT_TARGET
if "android_api_level" not in ARGUMENTS:
    ARGUMENTS["android_api_level"] = "28"

# Recent godot-cpp versions disables exceptions by default, but libdatachannel requires them.
ARGUMENTS["disable_exceptions"] = "no"

if env["godot_version"] == "4.0":
    sconstruct = env.get("godot_cpp", "godot-cpp-4.0") + "/SConstruct"
    cpp_env = SConscript(sconstruct)
    env = cpp_env.Clone()
else:
    sconstruct = env.get("godot_cpp", "godot-cpp") + "/SConstruct"
    cpp_env = SConscript(sconstruct)
    env = cpp_env.Clone()

if cpp_env.get("is_msvc", False):
    # Make sure we don't build with static cpp on MSVC (default in recent godot-cpp versions).
    replace_flags(env["CCFLAGS"], {"/MT": "/MD"})
    replace_flags(cpp_env["CCFLAGS"], {"/MT": "/MD"})

# Should probably go to upstream godot-cpp.
# We let SCons build its default ENV as it includes OS-specific things which we don't
# want to have to pull in manually.
# Then we prepend PATH to make it take precedence, while preserving SCons' own entries.
env.PrependENVPath("PATH", os.getenv("PATH"))
env.PrependENVPath("PKG_CONFIG_PATH", os.getenv("PKG_CONFIG_PATH"))
if "TERM" in os.environ:  # Used for colored output.
    env["ENV"]["TERM"] = os.environ["TERM"]

# Patch mingw SHLIBSUFFIX.
if env["platform"] == "windows" and env["use_mingw"]:
    env["SHLIBSUFFIX"] = ".dll"

# Patch OSXCross config.
if env["platform"] == "macos" and os.environ.get("OSXCROSS_ROOT", ""):
    env["SHLIBSUFFIX"] = ".dylib"
    if env["macos_deployment_target"] != "default":
        env["ENV"]["MACOSX_DEPLOYMENT_TARGET"] = env["macos_deployment_target"]

# Add option to increase build version
env["versioned"] = GetOption("versioned")

if "dev_build" in ARGUMENTS:
    env["dev_build"] = True

opts.Update(env)

target = env["target"]
if env["godot_version"] == "3":
    result_path = os.path.join(build_folder, "gdnative", "webrtc" if env["target"] == "release" else "webrtc_debug")
elif env["godot_version"] == "4.0":
    result_path = os.path.join(build_folder, "extension-4.0", "webrtc")
else:
    result_path = os.path.join(build_folder, "extension-4.1", "webrtc")
    #result_path = os.path.join("demo", "BomberMultiplayerDemo", "webrtc")

# Our includes and sources
env.Append(CPPPATH=["src/"])
env.Append(CPPDEFINES=["RTC_STATIC"])
env.Append(CPPPATH=["#thirdparty/pionc/include"])
if env["platform"] == "macos":
    lib_pion = File('thirdparty/pionc/lib/libwebrtc_universal.a')
elif env["platform"] == "windows":
    lib_pion = File('thirdparty/pionc/lib/libwebrtc.lib')
else:
    lib_pion = File('thirdparty/pionc/lib/libwebrtc.a')
env.Append(LIBS=[lib_pion])

env.Append(CPPPATH=["#thirdparty/opus/include"])
if env["platform"] == "windows":
    #lib_opus = File('thirdparty/opus/build/Release/opus.lib')
    if env["use_mingw"]:
        lib_opus = File('thirdparty/opus/.libs/libopus.a')
    else:
        lib_opus = File('thirdparty/opus/build/opus.lib')
else:
    lib_opus = File('thirdparty/opus/.libs/libopus.a')
env.Append(LIBS=[lib_opus])

sources = []
sources.append(
    [
        "src/WebRTCLibDataChannel.cpp",
        "src/WebRTCLibPeerConnection.cpp",
        "src/OpusEncoder.cpp",
        "src/OpusDecoder.cpp",
        "src/WavWriter.cpp"
    ]
)
if env["godot_version"] == "3":
    env.Append(CPPDEFINES=["GDNATIVE_WEBRTC"])
    sources.append("src/init_gdnative.cpp")
    add_sources(sources, "src/net/", "cpp")
else:
    sources.append("src/init_gdextension.cpp")
    if env["godot_version"] == "4.0":
        env.Append(CPPDEFINES=["GDEXTENSION_WEBRTC_40"])

# Add our build tools
# for tool in ["openssl", "cmake", "rtc"]:
for tool in ["opus"]:
    env.Tool(tool, toolpath=["tools"])

#ssl = env.OpenSSL()
#rtc = env.BuildLibDataChannel(ssl)

opus = env.Opus()

# Forces building our sources after OpenSSL and libdatachannel.
# This is because OpenSSL headers are generated by their build system and SCons doesn't know about them.
# Note: This might not be necessary in this specific case since our sources doesn't include OpenSSL headers directly,
# but it's better to be safe in case of indirect inclusions by one of our other dependencies.
#env.Depends(sources, ssl + rtc)

# We want to statically link against libstdc++ on Linux to maximize compatibility, but we must restrict the exported
# symbols using a GCC version script, or we might end up overriding symbols from other libraries.
# Using "-fvisibility=hidden" will not work, since libstdc++ explicitly exports its symbols.
symbols_file = None
if env["platform"] == "linux" or (
    env["platform"] == "windows" and env.get("use_mingw", False) and not env.get("use_llvm", False)
):
    if env["godot_version"] == "3":
        symbols_file = env.File("misc/gcc/symbols-gdnative.map")
    else:
        symbols_file = env.File("misc/gcc/symbols-extension.map")
    env.Append(
        LINKFLAGS=[
            "-Wl,--no-undefined,--version-script=" + symbols_file.abspath,
            "-static-libgcc",
            "-static-libstdc++",
        ]
    )
    env.Depends(sources, symbols_file)

#print(env.Dump())
print("Godot version: %s" % env["godot_version"])
print("LIBS is: %s" % env['LIBS'])
print("CC: %s" % env["CC"])
print("CCFLAGS: %s" % env["CCFLAGS"])
print("macos_deployment_target: %s" % env["macos_deployment_target"])
print("Versioned build: %s" % str(env["versioned"]))
print("SCONS_CACHE: %s" % os.environ.get("SCONS_CACHE"))

# Make the shared library
result_name = "libwebrtc_native{}{}".format(env["suffix"], env["SHLIBSUFFIX"])
if env["godot_version"] != "3" and env["platform"] == "macos":
    framework_path = os.path.join(
        result_path, "lib", "libwebrtc_native.macos.{}.{}.framework".format(env["target"], env["arch"])
    )
    library_file = env.SharedLibrary(target=os.path.join(framework_path, result_name), source=sources)
    plist_file = env.Substfile(
        os.path.join(framework_path, "Resources", "Info.plist"),
        "misc/dist/macos/Info.plist",
        SUBST_DICT={"{LIBRARY_NAME}": result_name, "{DISPLAY_NAME}": "libwebrtc_native" + env["suffix"]},
    )
    library = [library_file, plist_file]
else:
    library = env.SharedLibrary(target=os.path.join(result_path, "lib", result_name), source=sources)

Default(library)

# GDNativeLibrary
if env["godot_version"] == "3":
    gdnlib = "webrtc" if target != "debug" else "webrtc_debug"
    ext = ".tres"
    extfile = env.Substfile(
        os.path.join(result_path, gdnlib + ext),
        "misc/webrtc" + ext,
        SUBST_DICT={
            "{GDNATIVE_PATH}": gdnlib,
            "{TARGET}": "template_" + env["target"],
        },
    )
else:
    extfile = env.Substfile(
        os.path.join(result_path, "webrtc.gdextension"),
        "misc/webrtc.gdextension",
        SUBST_DICT={"{GODOT_VERSION}": env["godot_version"]},
    )

Default(extfile)

if env["versioned"]:
    version_file = "version.txt"
    increament_build_version(version_file)

#print(env.Dump())
debug_env = env.Clone()
# replace_flags(debug_env["CCFLAGS"], {"-O2": "-g"})
# debug_env["CCFLAGS"] += "-glldb"
# debug_env["CCFLAGS"] += "-gembed-source -ggnu-pubnames -g3 -gpubnames"
print("debug_env: CCFLAGS: %s" % debug_env["CCFLAGS"])
tests_files = []
add_sources(tests_files, "tests/",  "cpp")
test_app_name = 'test'
test_app_path = os.path.join('build', 'tests')
#test_app_path = 'tests'
test_app = debug_env.Program(target=os.path.join(test_app_path, test_app_name), source=tests_files)
Default(test_app)
