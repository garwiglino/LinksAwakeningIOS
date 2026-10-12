#!/usr/bin/env python3
"""Deterministic Xcode project, without XcodeGen, CocoaPods or package downloads."""
import hashlib
import json
from pathlib import Path
import plistlib

ROOT = Path(__file__).resolve().parents[1]
objects = {}
def uid(name):
    return hashlib.sha1(name.encode()).hexdigest()[:24].upper()
def obj(identity, **value):
    objects[uid(identity)] = value
    return uid(identity)
def emit(value, indent=0):
    if isinstance(value, dict):
        return "{\n" + "".join("\t" * (indent + 1) + json.dumps(k) + " = " + emit(v, indent+1) + ";\n" for k,v in value.items()) + "\t" * indent + "}"
    if isinstance(value, list):
        return "(" + ", ".join(emit(x, indent) for x in value) + ",)" if value else "()"
    return json.dumps(str(value))

source_paths = sorted(p.relative_to(ROOT).as_posix() for p in (ROOT / "Sources").rglob("*") if p.suffix in {".swift", ".c", ".metal"})
source_paths += sorted(p.relative_to(ROOT).as_posix() for p in (ROOT / "Vendor/GBRuntime/src").glob("*.c"))
generated = [f"DerivedSources/rom_part_{i:02d}.c" for i in range(32)] + ["DerivedSources/rom_rom.c"]
source_paths += generated
headers = ["DerivedSources/rom.h", "DerivedSources/rom_functions.h", "DerivedSources/GameIdentity.h", "Sources/Core/DXCore.h", "Sources/iOS/BridgingHeader.h"]
extra = headers + ["Resources/Info.plist", "Resources/PrivacyInfo.xcprivacy", "Config/Base.xcconfig", "README.md"]
refs = {}
for path in source_paths + extra:
    extension = Path(path).suffix
    kind = {".swift": "sourcecode.swift", ".c": "sourcecode.c.c", ".h": "sourcecode.c.h", ".metal": "sourcecode.metal", ".xcconfig": "text.xcconfig", ".plist": "text.plist.xml", ".xcprivacy": "text.plist.xml"}.get(extension, "text")
    refs[path] = obj("file:"+path, isa="PBXFileReference", lastKnownFileType=kind, path=path, sourceTree="SOURCE_ROOT")
builds = []
for path in source_paths:
    entry = dict(isa="PBXBuildFile", fileRef=refs[path])
    if path in generated:
        entry["settings"] = {"COMPILER_FLAGS": "-O1 -fno-inline -fno-strict-aliasing -Wno-unused-label -Wno-unused-function"}
    builds.append(obj("build:"+path, **entry))
privacy = obj("build:privacy", isa="PBXBuildFile", fileRef=refs["Resources/PrivacyInfo.xcprivacy"])
product = obj("product", isa="PBXFileReference", explicitFileType="wrapper.application", includeInIndex=0, path="LinksAwakeningDX.app", sourceTree="BUILT_PRODUCTS_DIR")
products = obj("products", isa="PBXGroup", children=[product], name="Products", sourceTree="<group>")
group = obj("group", isa="PBXGroup", children=list(refs.values())+[products], sourceTree="<group>")
preflight = obj("preflight", isa="PBXShellScriptBuildPhase", alwaysOutOfDate=1, buildActionMask=2147483647, files=[], inputPaths=[], outputPaths=["$(SRCROOT)/" + p for p in generated+headers[:3]+["DerivedSources/manifest.json"]], runOnlyForDeploymentPostprocessing=0, shellPath="/bin/sh", shellScript='set -eu\n/usr/bin/python3 "$SRCROOT/Scripts/ensure_generated.py"\n', name="Prepare and verify DX ROM")
sources = obj("sources", isa="PBXSourcesBuildPhase", buildActionMask=2147483647, files=builds, runOnlyForDeploymentPostprocessing=0)
frameworks = obj("frameworks", isa="PBXFrameworksBuildPhase", buildActionMask=2147483647, files=[], runOnlyForDeploymentPostprocessing=0)
resources = obj("resources", isa="PBXResourcesBuildPhase", buildActionMask=2147483647, files=[privacy], runOnlyForDeploymentPostprocessing=0)
for scope in ["project", "target"]:
    configs = []
    for name in ["Debug", "Release"]:
        settings = {}
        if scope == "project":
            settings = {"GCC_OPTIMIZATION_LEVEL": "0" if name == "Debug" else "2", "SWIFT_OPTIMIZATION_LEVEL": "-Onone" if name == "Debug" else "-O", "DEBUG_INFORMATION_FORMAT": "dwarf" if name == "Debug" else "dwarf-with-dsym", "SWIFT_ACTIVE_COMPILATION_CONDITIONS": "DEBUG" if name == "Debug" else "", "ENABLE_TESTABILITY": "YES" if name == "Debug" else "NO"}
        configs.append(obj(scope+name, isa="XCBuildConfiguration", baseConfigurationReference=refs["Config/Base.xcconfig"], buildSettings=settings, name=name))
    obj(scope+"configs", isa="XCConfigurationList", buildConfigurations=configs, defaultConfigurationIsVisible=0, defaultConfigurationName="Release")
target = obj("target", isa="PBXNativeTarget", buildConfigurationList=uid("targetconfigs"), buildPhases=[preflight,sources,frameworks,resources], buildRules=[], dependencies=[], name="LinksAwakeningDX", productName="LinksAwakeningDX", productReference=product, productType="com.apple.product-type.application")
project = obj("project", isa="PBXProject", attributes={"BuildIndependentTargetsInParallel": "YES", "LastUpgradeCheck": "1600", "TargetAttributes": {target:{"CreatedOnToolsVersion": "16.0", "ProvisioningStyle": "Automatic"}}}, buildConfigurationList=uid("projectconfigs"), compatibilityVersion="Xcode 14.0", developmentRegion="fr", hasScannedForEncodings=0, knownRegions=["fr","en","Base"], mainGroup=group, productRefGroup=products, projectDirPath="", projectRoot="", targets=[target])
folder = ROOT / "LinksAwakeningIOS.xcodeproj"
folder.mkdir(exist_ok=True)
(folder / "project.pbxproj").write_text("// !$*UTF8*$!\n"+emit({"archiveVersion":1,"classes":{},"objectVersion":56,"objects":objects,"rootObject":project})+"\n")
schemes = folder / "xcshareddata/xcschemes"
schemes.mkdir(parents=True,exist_ok=True)
ref = f'<BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="LinksAwakeningDX.app" BlueprintName="LinksAwakeningDX" ReferencedContainer="container:LinksAwakeningIOS.xcodeproj"/>'
(schemes / "LinksAwakeningDX.xcscheme").write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="1600" version="1.3">
<BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES">{ref}</BuildActionEntry></BuildActionEntries></BuildAction>
<TestAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv="YES"><Testables/></TestAction>
<LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="NO" debugDocumentVersioning="YES" debugServiceExtension="internal" allowLocationSimulation="YES"><BuildableProductRunnable runnableDebuggingMode="0">{ref}</BuildableProductRunnable></LaunchAction>
<ProfileAction buildConfiguration="Release" shouldUseLaunchSchemeArgsEnv="YES" savedToolIdentifier="" useCustomWorkingDirectory="NO" debugDocumentVersioning="YES"><BuildableProductRunnable runnableDebuggingMode="0">{ref}</BuildableProductRunnable></ProfileAction>
<AnalyzeAction buildConfiguration="Debug"/><ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/>
</Scheme>
''')
print("Xcode project generated:", len(source_paths), "sources")
