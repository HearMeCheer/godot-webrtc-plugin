import os, sys
import SCons.Util
import SCons.Builder
import SCons.Action
from SCons.Defaults import Mkdir
from SCons.Variables import PathVariable, BoolVariable

def opus_emitter(target, source, env):
    return env["SSL_LIBS"], [env.File(env["SSL_SOURCE"] + "/Configure"), env.File(env["SSL_SOURCE"] + "/VERSION.dat")]

def opus_generator(target, source, env, for_signature):
    # Strip the -j option for signature to avoid rebuilding when num_jobs changes.
    build = env["SSLBUILDCOM"].replace("-j$SSLBUILDJOBS", "") if for_signature else env["SSLBUILDCOM"]
    return [
        Mkdir("$SSL_BUILD"),
        Mkdir("$SSL_INSTALL"),
        SCons.Action.Action("$SSLCONFIGCOM", "$SSLCONFIGCOMSTR"),
        SCons.Action.Action(build, "$SSLBUILDCOMSTR"),
    ]

def build_opus(env, jobs=None):

    if jobs is None:
        jobs = int(env.GetOption("num_jobs"))
    
    pass

def options(opts):
    opts.Add(PathVariable("opus_source", "Path to the opus sources.", "thirdparty/opus"))
    opts.Add("opus_build", "Destination path of the opus build.", "build/bin/thirdparty/opus")

def exists(env):
    return True

def generate(env):
    env.AddMethod(build_opus, "Opus")

    env["OPUS_SOURCE"] = env.Dir(env["opus_source"]).abspath
    env["OPUS_BUILD"] = env.Dir(env["opus_build"] + "/{}/{}".format(env["platform"], env["arch"])).abspath

    print("OPUS_SOURCE: ", env["OPUS_SOURCE"])
    print("OPUS_BUILD: ", env["OPUS_BUILD"])

    env["BUILDERS"]["OpenSSLBuilder"] = SCons.Builder.Builder(generator=opus_generator, emitter=opus_emitter)
    #env.AddMethod(build_opus, "Opus")