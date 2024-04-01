import os
import platform
import sys
import os.path

import methods

def is_active():
    return True


def get_name():
    return "Playstation 2"


def can_build():
     # Check the minimal dependencies
    if "PS2SDK" not in os.environ:
        print("PS2SDK not defined in environment.. Playstation 2 disabled.")
        return False
    
    if "PS2DEV" not in os.environ:
        print("PS2DEV not defined in environment.. Playstation 2 disabled.")
        return False
    
    print("Playstation 2 is available!")
    
    return True


def get_opts():
    return []


def get_flags():
    return [
        ("tools", False),
        ("bits", "32"),
    ]


def configure(env):
    env["CC"] = "mips64r5900el-ps2-elf-gcc"
    env["CXX"] = "mips64r5900el-ps2-elf-g++"
    env["AS"] = "mips64r5900el-ps2-elf-as"
    env["LD"] = "mips64r5900el-ps2-elf-ld"
    env["AR"] = "mips64r5900el-ps2-elf-ar"
    env["OBJCOPY"] = "mips64r5900el-ps2-elf-objcopy"
    env["STRIP"] = "mips64r5900el-ps2-elf-strip"
    env["ADDR2LINE"] = "mips64r5900el-ps2-elf-addr2line"
    env["RANLIB"] = "mips64r5900el-ps2-elf-ranlib"

    PS2DEV = os.environ.get("PS2DEV")

    env.Prepend(CPPPATH=[os.path.join(PS2DEV, "ps2sdk/ee/include"),
                         os.path.join(PS2DEV, "ps2sdk/common/include"),
                         os.path.join(PS2DEV, "ps2sdk/ports/include")])
    
    env.Append(CCFLAGS=['-D_EE', '-D__PS2__'])
    env.Append(ASFLAGS=['-O3'])

    env.Append(LIBS=[
        "stdc++",
        "dma", 
        "packet2", 
        "graph", 
        "draw", 
        "math3d",
        "pad", 
        "audsrv", 
        "patches", 
        "cdvd",
        "debug",
    ])
    env.Append(
        LINKFLAGS=[
            "-Wl,-zmax-page-size=128",
            "-L{}/ps2sdk/ee/lib".format(PS2DEV),
            "-L{}/ports/lib".format(PS2DEV),
            '-T{}/ps2sdk/ee/startup/linkfile'.format(PS2DEV)
        ]
    )

    if (env["target"] == "release"):
        env.Prepend(CCFLAGS=['-Ofast'])
        if (env["debug_release"] == "yes"):
            env.Prepend(CCFLAGS=['-g2'])

    elif (env["target"] == "release_debug"):
        env.Prepend(CCFLAGS=['-O2', '-ffast-math', '-DDEBUG_ENABLED'])
        if (env["debug_release"] == "yes"):
            env.Prepend(CCFLAGS=['-g2'])

    elif (env["target"] == "debug"):
        env.Prepend(CCFLAGS=['-g2', '-DDEBUG_ENABLED', '-DDEBUG_MEMORY_ENABLED'])

    env.Append(CPPDEFINES=['NEED_LONG_INT', 'NO_THREADS', 'DEBUG_INIT'])
    env.Prepend(CPPPATH=["#platform/ps2"])

    print("CCFLAGS=" + str(env["CCFLAGS"]))
    print("LINKFLAGS=" + str(env["LINKFLAGS"]))

    env["module_cscript_enabled"] = "no"
    env["module_dds_enabled"] = "no"
    env["module_mpc_enabled"] = "no"
    env["module_openssl_enabled"] = "no"
    env["module_opus_enabled"] = "no"
    env["module_pvr_enabled"] = "no"
    env["module_speex_enabled"] = "no"
    env["module_theora_enabled"] = "no"
    env["module_webp_enabled"] = "no"
    env["module_squish_enabled"] = "no"