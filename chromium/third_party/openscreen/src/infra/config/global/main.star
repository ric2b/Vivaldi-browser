#!/usr/bin/env lucicfg
"""
Open Screen's LUCI configuration for post-submit and pre-submit builders.
"""

REPO_URL = "https://chromium.googlesource.com/openscreen"
CHROMIUM_REPO_URL = "https://chromium.googlesource.com/chromium/src"
MAC_VERSION = "Mac-13"
WINDOWS_VERSION = "Windows-10"
REF = "refs/heads/main"

RECLIENT_PROPERTY = "$build/reclient"

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

luci.project(
    name = "openscreen",
    milo = "luci-milo.appspot.com",
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog.appspot.com",
    swarming = "chromium-swarm.appspot.com",
    scheduler = "luci-scheduler.appspot.com",
    acls = [
        acl.entry(
            roles = [
                acl.BUILDBUCKET_READER,
                acl.SCHEDULER_READER,
                acl.PROJECT_CONFIGS_READER,
                acl.LOGDOG_READER,
            ],
            groups = "all",
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = "project-openscreen-admins",
        ),
        acl.entry(
            roles = acl.LOGDOG_WRITER,
            groups = "luci-logdog-chromium-writers",
        ),
        acl.entry(
            roles = acl.CQ_COMMITTER,
            groups = "project-openscreen-committers",
        ),
        acl.entry(
            roles = acl.CQ_DRY_RUNNER,
            groups = "project-openscreen-tryjob-access",
        ),
    ],
)

luci.milo(
    logo = ("https://storage.googleapis.com/chrome-infra-public/logo/" +
            "openscreen-logo.png"),
)

luci.logdog(gs_bucket = "chromium-luci-logdog")

# Gitiles pollers are used for triggering CI builders.
luci.gitiles_poller(
    name = "main-gitiles-trigger",
    bucket = "ci",
    repo = REPO_URL,
)
luci.gitiles_poller(
    name = "chromium-trigger",
    bucket = "ci",
    repo = CHROMIUM_REPO_URL,
)

# Whereas tryjob verifiers are used for triggering try builders.
luci.cq_group(
    name = "openscreen-build-config",
    watch = cq.refset(
        repo = REPO_URL,
        refs = ["refs/heads/.+"],
    ),
)
luci.cq(status_host = "chromium-cq-status.appspot.com")

luci.bucket(
    name = "try",
    acls = [
        acl.entry(
            roles = [acl.BUILDBUCKET_TRIGGERER, acl.CQ_COMMITTER],
            groups = [
                "project-openscreen-tryjob-access",
                "service-account-cq",
            ],
        ),
    ],
)
luci.bucket(
    name = "ci",
    acls = [
        acl.entry(
            roles = [acl.BUILDBUCKET_TRIGGERER],
            users = "luci-scheduler@appspot.gserviceaccount.com",
        ),
    ],
)

luci.console_view(
    name = "ci",
    title = "OpenScreen CI Builders",
    repo = REPO_URL,
)
luci.console_view(
    name = "try",
    title = "OpenScreen Try Builders",
    repo = REPO_URL,
)

_reclient = struct(
    instance = struct(
        DEFAULT_TRUSTED = "rbe-chromium-trusted",
        DEFAULT_UNTRUSTED = "rbe-chromium-untrusted",
    ),
)

def get_properties(
        is_debug = True,
        is_gcc = False,
        is_asan = False,
        is_tsan = False,
        is_msan = False,
        use_coverage = False,
        target_cpu = "x64",
        cast_receiver = False,
        chromium = False,
        is_presubmit = False,
        is_component_build = None,
        is_ci = None):
    """Property generator method, used to configure the build system.

    Args:
      is_debug: if False, the build mode is release instead of debug.
      is_gcc: if True, the GCC compiler is used instead of clang.
      is_asan: if True, this is an address sanitizer build.
      is_msan: if True, this is a memory sanitizer build.
      is_tsan: if True, this is a thread sanitizer build.
      use_coverage: if True, this is a code coverage build.
      target_cpu: the target CPU. May differ from current_cpu or host_cpu
        if cross compiling.
      cast_receiver: if True, this build should include the cast standalone
        sender and receiver binaries.
      chromium: if True, the build is for use in an embedder, such as Chrome.
      is_presubmit: if True, this is a presubmit run.
      is_component_build: if set, enables or disables component builds.
      is_ci: If set, it adds is_ci flag to the properties.

    Returns:
        A collection of GN properties for the build system.
    """
    properties = {
        "clang_use_chrome_plugins": False,
        "target_cpu": target_cpu,
        "$recipe_engine/swarming": {
            "server": "https://chromium-swarm.appspot.com",
        },
    }
    if not is_debug:
        properties["is_debug"] = False
    if is_gcc:
        properties["is_clang"] = False
        properties["use_custom_libcxx"] = False
    if is_asan:
        properties["is_asan"] = True
    if is_msan:
        properties["is_msan"] = True
    if is_tsan:
        properties["is_tsan"] = True
    if use_coverage:
        properties["use_coverage"] = True
    if cast_receiver:
        properties["have_ffmpeg"] = True
        properties["have_libsdl2"] = True
        properties["have_libopus"] = True
        properties["have_libvpx"] = True
    if chromium:
        properties["builder_group"] = "client.openscreen.chromium"
        properties[RECLIENT_PROPERTY] = {
            "instance": _reclient.instance.DEFAULT_UNTRUSTED,
            "metrics_project": "chromium-reclient-metrics",
            "scandeps_server": True,
        }

    if is_presubmit:
        properties["repo_name"] = "openscreen"
        properties["runhooks"] = "true"

    if is_component_build != None:
        properties["is_component_build"] = is_component_build

    if is_ci:
        properties["is_ci"] = is_ci
    return properties

def builder(builder_type, name, properties, os, cpu):
    """Defines a builder.

    Args:
      builder_type: "ci" or "try".
      name: name of the builder to define.
      properties: configuration to be passed to GN.
      os: the target operating system.
      cpu: the target architecture, such as "arm64."
    """
    recipe_id = "openscreen"
    use_python3 = True
    if properties:
        if "builder_group" in properties:
            recipe_id = "chromium"
        elif "runhooks" in properties:
            recipe_id = "run_presubmit"

    caches = []
    if os == MAC_VERSION:
        caches.append(swarming.cache("osx_sdk"))

    triggers = None
    if builder_type == "ci":
        triggers = ["chromium-trigger" if recipe_id == "chromium" else "main-gitiles-trigger"]

    luci.builder(
        name = name,
        bucket = builder_type,
        executable = luci.recipe(
            name = recipe_id,
            recipe = recipe_id,
            cipd_package =
                "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
            cipd_version = "refs/heads/main",
            use_bbagent = True,
            use_python3 = use_python3,
        ),
        dimensions = {
            "pool": "luci.flex." + builder_type,
            "os": os,
            "cpu": cpu,
        },
        caches = caches,
        properties = properties,
        service_account =
            "openscreen-{}-builder@chops-service-accounts.iam.gserviceaccount.com"
                .format(builder_type),
        triggered_by = triggers,
    )

    # CI jobs get triggered by |triggers|, try jobs get trigged by the commit
    # queue instead.
    if builder_type == "try":
        # We mark some bots as experimental to not block the build.
        experiment_percentage = None
        if name in [
            "linux_arm64",
            "linux_arm64_cast_receiver",
            "linux_x64_coverage",
            "win_x64",
            "chromium_win_x64",
        ]:
            experiment_percentage = 100

        luci.cq_tryjob_verifier(
            builder = "try/" + name,
            cq_group = "openscreen-build-config",
            experiment_percentage = experiment_percentage,
        )

    luci.console_view_entry(
        builder = "{}/{}".format(builder_type, name),
        console_view = builder_type,
        category = "{}|{}".format(os, cpu),
        short_name = name,
    )

def ci_builder(name, properties, os = "Ubuntu-20.04", cpu = "x86-64"):
    """Defines a post submit builder.

       Args:
        name: name of the builder to define.
        properties: configuration to be passed to GN.
        os: the target operating system.
        cpu: the target central processing unit.
    """
    builder("ci", name, properties, os, cpu)

def try_builder(name, properties, os = "Ubuntu-20.04", cpu = "x86-64"):
    """Defines a pre submit builder.

    Args:
      name: name of the builder to define.
      properties: configuration to be passed to GN.
      os: the target operating system.
      cpu: the target central processing unit.
    """
    builder("try", name, properties, os, cpu)

def try_and_ci_builders(name, properties, os = "Ubuntu-20.04", cpu = "x86-64"):
    """Defines a similarly configured try and ci builder pair.

    Args:
      name: name of the builder to define.
      properties: configuration to be passed to GN.
      os: the target operating system.
      cpu: the target central processing unit.
    """
    try_builder(name, properties, os, cpu)

    ci_properties = dict(properties)
    ci_properties["is_ci"] = True
    RECLIENT_PROPERTY = "$build/reclient"
    if RECLIENT_PROPERTY in ci_properties:
        ci_properties[RECLIENT_PROPERTY] = dict(ci_properties[RECLIENT_PROPERTY])
        ci_properties[RECLIENT_PROPERTY]["instance"] = _reclient.instance.DEFAULT_TRUSTED
    ci_builder(name, ci_properties, os, cpu)

# BUILDER CONFIGURATIONS
# Follow the pattern: <platform>_<arch>
# For builders other than the generic debug config, use <platform>_<arch>_<config>
# For Chromium builders, use chromium_<platform>_<arch>_<config>

try_builder(
    "openscreen_presubmit",
    get_properties(is_presubmit = True, is_debug = False),
)
try_and_ci_builders(
    "linux_arm64_cast_receiver",
    get_properties(cast_receiver = True, target_cpu = "arm64", is_component_build = False),
)
try_and_ci_builders("linux_x64_coverage", get_properties(use_coverage = True))
try_and_ci_builders("linux_x64", get_properties(is_asan = True))
try_and_ci_builders(
    "linux_x64_gcc",
    get_properties(is_gcc = True),
)
try_and_ci_builders(
    "linux_x64_msan_rel",
    get_properties(is_debug = False, is_msan = True),
)
try_and_ci_builders(
    "linux_x64_tsan_rel",
    get_properties(is_debug = False, is_tsan = True),
)
try_and_ci_builders(
    "linux_arm64",
    get_properties(target_cpu = "arm64", is_component_build = False),
)
try_and_ci_builders("mac_x64", get_properties(), os = MAC_VERSION)
try_and_ci_builders(
    "win_x64",
    get_properties(),
    os = WINDOWS_VERSION,
)
try_and_ci_builders(
    "chromium_linux_x64",
    get_properties(chromium = True),
)
try_and_ci_builders(
    "chromium_mac_x64",
    get_properties(chromium = True),
    os = MAC_VERSION,
)
try_and_ci_builders(
    "chromium_win_x64",
    get_properties(chromium = True),
    os = WINDOWS_VERSION,
)
