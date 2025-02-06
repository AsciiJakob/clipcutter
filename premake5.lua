include("premake-ecc/ecc.lua")

workspace "clipcutter"
    configurations { "Debug", "Release" }
    architecture "x86_64"

    include "clipcutter"