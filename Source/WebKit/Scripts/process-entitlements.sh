#!/bin/bash
set -e

function plistbuddy()
{
    /usr/libexec/PlistBuddy -c "$*" "${WK_PROCESSED_XCENT_FILE}"
}

# ========================================
# Mac entitlements
# ========================================

function mac_process_webcontent_entitlements()
{
    plistbuddy Add :com.apple.security.cs.allow-jit bool YES
    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
        plistbuddy Add :com.apple.rootless.storage.WebKitWebContentSandbox bool YES
        plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.developer.kernel.extended-virtual-addressing bool YES
            plistbuddy Add :com.apple.developer.videotoolbox.client-sandboxed-decoder bool YES
            plistbuddy Add :com.apple.pac.shared_region_id string WebContent
            plistbuddy Add :com.apple.private.pac.exception bool YES
            plistbuddy Add :com.apple.private.security.message-filter bool YES
            plistbuddy Add :com.apple.avfoundation.allow-system-wide-context bool YES
            plistbuddy add :com.apple.QuartzCore.webkit-limited-types bool YES
        fi
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 130000 ))
        then
            plistbuddy Add :com.apple.private.gpu-restricted bool YES
        fi
    fi

    mac_process_webcontent_shared_entitlements
}

function mac_process_webcontent_captiveportal_entitlements()
{
    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit
    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
        plistbuddy Add :com.apple.rootless.storage.WebKitWebContentSandbox bool YES
        plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES

        plistbuddy Add :com.apple.imageio.allowabletypes array
        plistbuddy Add :com.apple.imageio.allowabletypes:0 string org.webmproject.webp
        plistbuddy Add :com.apple.imageio.allowabletypes:1 string public.jpeg
        plistbuddy Add :com.apple.imageio.allowabletypes:2 string public.png
        plistbuddy Add :com.apple.imageio.allowabletypes:3 string com.compuserve.gif

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.developer.kernel.extended-virtual-addressing bool YES
            plistbuddy Add :com.apple.developer.videotoolbox.client-sandboxed-decoder bool YES
            plistbuddy Add :com.apple.pac.shared_region_id string WebContent
            plistbuddy Add :com.apple.private.pac.exception bool YES
            plistbuddy Add :com.apple.private.security.message-filter bool YES
            plistbuddy Add :com.apple.avfoundation.allow-system-wide-context bool YES
            plistbuddy add :com.apple.QuartzCore.webkit-limited-types bool YES
        fi
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 130000 ))
        then
            plistbuddy Add :com.apple.private.gpu-restricted bool YES
        fi
    fi

    mac_process_webcontent_shared_entitlements
}

function mac_process_gpu_entitlements()
{
    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 101400 ))
        then
            plistbuddy Add :com.apple.tcc.delegated-services array
            plistbuddy Add :com.apple.tcc.delegated-services:1 string kTCCServiceMicrophone
            plistbuddy Add :com.apple.tcc.delegated-services:0 string kTCCServiceCamera
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.developer.videotoolbox.client-sandboxed-decoder bool YES
            plistbuddy Add :com.apple.avfoundation.allow-system-wide-context bool YES
            plistbuddy add :com.apple.QuartzCore.webkit-limited-types bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.private.security.message-filter bool YES
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 ))
        then
            plistbuddy add :com.apple.coreaudio.allow-vorbis-decode bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 130000 ))
        then
            plistbuddy Add :com.apple.private.gpu-restricted bool YES
            plistbuddy Add :com.apple.private.screencapturekit.sharingsession bool YES
            plistbuddy Add :com.apple.private.tcc.allow array
            plistbuddy Add :com.apple.private.tcc.allow:0 string kTCCServiceScreenCapture
            plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES
            plistbuddy Add :com.apple.mediaremote.external-artwork-validation bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 140000 ))
        then
            plistbuddy add :com.apple.private.disable.screencapturekit.alert bool YES
            plistbuddy add :com.apple.aneuserd.private.allow bool YES
        fi

        plistbuddy Add :com.apple.private.memory.ownership_transfer bool YES
        plistbuddy Add :com.apple.private.pac.exception bool YES
        plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
        plistbuddy Add :com.apple.rootless.storage.WebKitGPUSandbox bool YES
        plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
        plistbuddy Add :com.apple.private.coremedia.allow-fps-attachment bool YES
        plistbuddy Add :com.apple.developer.hardened-process bool YES
    fi
}

function mac_process_network_entitlements()
{
    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 101500 ))
        then
            plistbuddy Add :com.apple.private.network.socket-delegate bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.private.security.message-filter bool YES
            plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token array
            plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token:0 string kTCCServiceWebKitIntelligentTrackingPrevention
            plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token:1 string kTCCServiceUserTracking
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 130000 ))
        then
            plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES
            plistbuddy Add :com.apple.private.assets.bypass-asset-types-check bool YES
            plistbuddy Add :com.apple.private.assets.accessible-asset-types array
            plistbuddy Add :com.apple.private.assets.accessible-asset-types:0 string com.apple.MobileAsset.WebContentRestrictions
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 140000 ))
        then
            plistbuddy Add :com.apple.private.ciphermld.allow bool YES
        fi

        plistbuddy Add :com.apple.private.launchservices.allowedtochangethesekeysinotherapplications array
        plistbuddy Add :com.apple.private.launchservices.allowedtochangethesekeysinotherapplications:0 string LSActivePageUserVisibleOriginsKey
        plistbuddy Add :com.apple.private.launchservices.allowedtochangethesekeysinotherapplications:1 string LSDisplayName
        plistbuddy Add :com.apple.private.launchservices.allowedtolaunchasproxy bool YES
        plistbuddy Add :com.apple.private.pac.exception bool YES
        plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
        plistbuddy Add :com.apple.rootless.storage.WebKitNetworkingSandbox bool YES
        plistbuddy Add :com.apple.symptom_analytics.configure bool YES
        plistbuddy Add :com.apple.private.webkit.adattributiond bool YES
        plistbuddy Add :com.apple.private.webkit.webpush bool YES
        plistbuddy Add :com.apple.developer.hardened-process bool YES
    fi
}

function webcontent_sandbox_entitlements()
{
    plistbuddy Add :com.apple.private.security.mutable-state-flags array
    plistbuddy Add :com.apple.private.security.mutable-state-flags:0 string EnableExperimentalSandbox
    plistbuddy Add :com.apple.private.security.mutable-state-flags:1 string BlockIOKitInWebContentSandbox
    plistbuddy Add :com.apple.private.security.mutable-state-flags:2 string local:WebContentProcessLaunched
    plistbuddy Add :com.apple.private.security.mutable-state-flags:3 string EnableQuickLookSandboxResources
    plistbuddy Add :com.apple.private.security.mutable-state-flags:4 string ParentProcessCanEnableQuickLookStateFlag
    plistbuddy Add :com.apple.private.security.mutable-state-flags:5 string BlockOpenDirectoryInWebContentSandbox
    plistbuddy Add :com.apple.private.security.mutable-state-flags:6 string BlockMobileAssetInWebContentSandbox
    plistbuddy Add :com.apple.private.security.mutable-state-flags:7 string BlockWebInspectorInWebContentSandbox
    plistbuddy Add :com.apple.private.security.mutable-state-flags:8 string BlockIconServicesInWebContentSandbox
    plistbuddy Add :com.apple.private.security.mutable-state-flags:9 string BlockFontServiceInWebContentSandbox
    plistbuddy Add :com.apple.private.security.mutable-state-flags:10 string UnifiedPDFEnabled
    plistbuddy Add :com.apple.private.security.mutable-state-flags:11 string WebProcessDidNotInjectStoreBundle
    plistbuddy Add :com.apple.private.security.enable-state-flags array
    plistbuddy Add :com.apple.private.security.enable-state-flags:0 string EnableExperimentalSandbox
    plistbuddy Add :com.apple.private.security.enable-state-flags:1 string BlockIOKitInWebContentSandbox
    plistbuddy Add :com.apple.private.security.enable-state-flags:2 string local:WebContentProcessLaunched
    plistbuddy Add :com.apple.private.security.enable-state-flags:3 string ParentProcessCanEnableQuickLookStateFlag
    plistbuddy Add :com.apple.private.security.enable-state-flags:4 string BlockOpenDirectoryInWebContentSandbox
    plistbuddy Add :com.apple.private.security.enable-state-flags:5 string BlockMobileAssetInWebContentSandbox
    plistbuddy Add :com.apple.private.security.enable-state-flags:6 string BlockWebInspectorInWebContentSandbox
    plistbuddy Add :com.apple.private.security.enable-state-flags:7 string BlockIconServicesInWebContentSandbox
    plistbuddy Add :com.apple.private.security.enable-state-flags:8 string BlockFontServiceInWebContentSandbox
    plistbuddy Add :com.apple.private.security.enable-state-flags:9 string UnifiedPDFEnabled
    plistbuddy Add :com.apple.private.security.enable-state-flags:10 string WebProcessDidNotInjectStoreBundle
}

function extract_notification_names() {
    perl -nle 'print "$1" if /WK_NOTIFICATION\("([^"]+)"\)/' < "$1"
}

function notify_entitlements()
{
    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        plistbuddy Add :com.apple.developer.web-browser-engine.restrict.notifyd bool YES
        plistbuddy Add :com.apple.private.darwin-notification.introspect array

        NOTIFICATION_INDEX=0

        extract_notification_names Resources/cocoa/NotificationAllowList/ForwardedNotifications.def | while read NOTIFICATION; do
            plistbuddy Add :com.apple.private.darwin-notification.introspect:$NOTIFICATION_INDEX string "$NOTIFICATION"
            NOTIFICATION_INDEX=$((NOTIFICATION_INDEX + 1))
        done

        if [[ "${WK_PLATFORM_NAME}" == macosx ]]
        then
            extract_notification_names Resources/cocoa/NotificationAllowList/MacForwardedNotifications.def | while read NOTIFICATION; do
                plistbuddy Add :com.apple.private.darwin-notification.introspect:$NOTIFICATION_INDEX string "$NOTIFICATION"
                NOTIFICATION_INDEX=$((NOTIFICATION_INDEX + 1))
            done
        else
            extract_notification_names Resources/cocoa/NotificationAllowList/EmbeddedForwardedNotifications.def | while read NOTIFICATION; do
                plistbuddy Add :com.apple.private.darwin-notification.introspect:$NOTIFICATION_INDEX string "$NOTIFICATION"
                NOTIFICATION_INDEX=$((NOTIFICATION_INDEX + 1))
            done
        fi

        extract_notification_names Resources/cocoa/NotificationAllowList/NonForwardedNotifications.def | while read NOTIFICATION; do
            plistbuddy Add :com.apple.private.darwin-notification.introspect:$NOTIFICATION_INDEX string "$NOTIFICATION"
            NOTIFICATION_INDEX=$((NOTIFICATION_INDEX + 1))
        done
    fi
}

function mac_process_webcontent_shared_entitlements()
{
    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 ))
        then
            plistbuddy Add :com.apple.private.verified-jit bool YES
            plistbuddy Add :com.apple.security.cs.single-jit bool YES
            plistbuddy add :com.apple.coreaudio.allow-vorbis-decode bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 130000 ))
        then
            webcontent_sandbox_entitlements
            plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES
        fi

        if [[ "${WK_WEBCONTENT_SERVICE_NEEDS_XPC_DOMAIN_EXTENSION_ENTITLEMENT}" == YES ]]
        then
            plistbuddy Add :com.apple.private.xpc.domain-extension bool YES
        fi

        plistbuddy Add :com.apple.developer.hardened-process bool YES
    fi

    if [[ "${WK_XPC_SERVICE_VARIANT}" == Development ]]
    then
        plistbuddy Add :com.apple.security.cs.disable-library-validation bool YES
    fi

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" > 140000 ))
    then
        notify_entitlements
    fi
}

function mac_process_webpushd_entitlements()
{
    plistbuddy Add :com.apple.private.aps-connection-initiate bool YES
    plistbuddy Add :com.apple.private.launchservices.entitledtoaccessothersessions bool YES
    plistbuddy Add :com.apple.usernotification.notificationschedulerproxy bool YES

    if [[ "${WK_RELOCATABLE_WEBPUSHD}" == NO ]]; then
        plistbuddy Add :com.apple.security.application-groups array
        plistbuddy Add :com.apple.security.application-groups:0 string group.com.apple.webkit.webpushd
        plistbuddy Add :com.apple.private.security.restricted-application-groups array
        plistbuddy Add :com.apple.private.security.restricted-application-groups:0 string group.com.apple.webkit.webpushd
    fi
}

# ========================================
# macCatalyst entitlements
# ========================================

function maccatalyst_process_webcontent_entitlements()
{
    plistbuddy Add :com.apple.security.cs.allow-jit bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES
    plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
    plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit
    plistbuddy Add :com.apple.developer.hardened-process bool YES

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
    then
        plistbuddy Add :com.apple.developer.kernel.extended-virtual-addressing bool YES
        plistbuddy Add :com.apple.pac.shared_region_id string WebContent
        plistbuddy Add :com.apple.private.pac.exception bool YES
        plistbuddy Add :com.apple.private.security.message-filter bool YES
        plistbuddy Add :com.apple.UIKit.view-service-wants-custom-idiom-and-scale bool YES
        plistbuddy Add :com.apple.QuartzCore.webkit-limited-types bool YES
    fi

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi
    fi

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 ))
    then
        plistbuddy Add :com.apple.private.verified-jit bool YES
        plistbuddy Add :com.apple.security.cs.single-jit bool YES
    fi

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" > 150000 ))
    then
        plistbuddy Add :com.apple.private.disable-log-mach-ports bool YES
    fi
}

function maccatalyst_process_webcontent_captiveportal_entitlements()
{
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES
    plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
    plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
    plistbuddy Add :com.apple.developer.hardened-process bool YES

    plistbuddy Add :com.apple.imageio.allowabletypes array
    plistbuddy Add :com.apple.imageio.allowabletypes:0 string org.webmproject.webp
    plistbuddy Add :com.apple.imageio.allowabletypes:1 string public.jpeg
    plistbuddy Add :com.apple.imageio.allowabletypes:2 string public.png
    plistbuddy Add :com.apple.imageio.allowabletypes:3 string com.compuserve.gif

    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
    then
        plistbuddy Add :com.apple.developer.kernel.extended-virtual-addressing bool YES
        plistbuddy Add :com.apple.pac.shared_region_id string WebContent
        plistbuddy Add :com.apple.private.pac.exception bool YES
        plistbuddy Add :com.apple.private.security.message-filter bool YES
        plistbuddy Add :com.apple.UIKit.view-service-wants-custom-idiom-and-scale bool YES
        plistbuddy Add :com.apple.QuartzCore.webkit-limited-types bool YES
    fi

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi
    fi

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 ))
    then
        plistbuddy Add :com.apple.private.verified-jit bool YES
        plistbuddy Add :com.apple.security.cs.single-jit bool YES
    fi
}

function maccatalyst_process_gpu_entitlements()
{
    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit
    plistbuddy Add :com.apple.security.network.client bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES
    plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
    plistbuddy Add :com.apple.private.memory.ownership_transfer bool YES
    plistbuddy Add :com.apple.private.pac.exception bool YES
    plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
    plistbuddy Add :com.apple.QuartzCore.webkit-limited-types bool YES
    plistbuddy Add :com.apple.private.coremedia.allow-fps-attachment bool YES
    plistbuddy Add :com.apple.developer.hardened-process bool YES

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi
    fi
}

function maccatalyst_process_network_entitlements()
{
    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit
    plistbuddy Add :com.apple.private.network.socket-delegate bool YES
    plistbuddy Add :com.apple.security.network.client bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES
    plistbuddy Add :com.apple.private.pac.exception bool YES
    plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
    plistbuddy Add :com.apple.developer.hardened-process bool YES

    plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token array
    plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token:0 string kTCCServiceWebKitIntelligentTrackingPrevention
    plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token:1 string kTCCServiceUserTracking

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi
    fi
}

# ========================================
# iOS Family entitlements
# ========================================

function ios_family_process_webcontent_shared_entitlements()
{
    plistbuddy Add :com.apple.QuartzCore.secure-mode bool YES
    plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
    plistbuddy add :com.apple.QuartzCore.webkit-limited-types bool YES
    plistbuddy Add :com.apple.developer.coremedia.allow-alternate-video-decoder-selection bool YES
    plistbuddy Add :com.apple.mediaremote.set-playback-state bool YES
    plistbuddy Add :com.apple.pac.shared_region_id string WebContent
    plistbuddy Add :com.apple.private.allow-explicit-graphics-priority bool YES
    plistbuddy Add :com.apple.private.coremedia.extensions.audiorecording.allow bool YES
    plistbuddy Add :com.apple.private.coremedia.pidinheritance.allow bool YES
    plistbuddy Add :com.apple.private.disable-log-mach-ports bool YES
    plistbuddy Add :com.apple.private.memorystatus bool YES
    plistbuddy Add :com.apple.private.network.socket-delegate bool YES
    plistbuddy Add :com.apple.private.webinspector.allow-remote-inspection bool YES
    plistbuddy Add :com.apple.private.webinspector.proxy-application bool YES
    plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES

    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit

if [[ "${PRODUCT_NAME}" != WebContentExtension && "${PRODUCT_NAME}" != WebContentCaptivePortalExtension ]]; then
    plistbuddy Add :com.apple.private.gpu-restricted bool YES
    plistbuddy Add :com.apple.private.pac.exception bool YES
    plistbuddy Add :com.apple.private.sandbox.profile string com.apple.WebKit.WebContent
fi
    plistbuddy add :com.apple.coreaudio.LoadDecodersInProcess bool YES
    plistbuddy add :com.apple.coreaudio.allow-vorbis-decode bool YES
    plistbuddy Add :com.apple.developer.hardened-process bool YES

    notify_entitlements
    webcontent_sandbox_entitlements
}

function ios_family_process_webcontent_entitlements()
{
    if [[ "${PLATFORM_NAME}" != watchos && "${PLATFORM_NAME}" != appletvos ]]; then
        plistbuddy Add :com.apple.private.verified-jit bool YES
        if [[ "${PLATFORM_NAME}" == iphoneos ]]; then
            if (( $(( ${SDK_VERSION_ACTUAL} )) >= 170400 )); then
                plistbuddy Add :com.apple.developer.cs.allow-jit bool YES
            else
                plistbuddy Add :dynamic-codesigning bool YES
            fi
        else
            plistbuddy Add :dynamic-codesigning bool YES
        fi
    fi
    plistbuddy Add :com.apple.developer.kernel.extended-virtual-addressing bool YES

    ios_family_process_webcontent_shared_entitlements
}

function ios_family_process_webcontent_captiveportal_entitlements()
{
    plistbuddy Add :com.apple.private.webkit.lockdown-mode bool YES

    plistbuddy Add :com.apple.developer.kernel.extended-virtual-addressing bool YES

    plistbuddy Add :com.apple.imageio.allowabletypes array
    plistbuddy Add :com.apple.imageio.allowabletypes:0 string org.webmproject.webp
    plistbuddy Add :com.apple.imageio.allowabletypes:1 string public.jpeg
    plistbuddy Add :com.apple.imageio.allowabletypes:2 string public.png
    plistbuddy Add :com.apple.imageio.allowabletypes:3 string com.compuserve.gif

    ios_family_process_webcontent_shared_entitlements
}

function ios_family_process_gpu_entitlements()
{
    plistbuddy add :application-identifier string "${PRODUCT_BUNDLE_IDENTIFIER}"

    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit

    plistbuddy Add :com.apple.QuartzCore.secure-mode bool YES
    plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
    plistbuddy add :com.apple.QuartzCore.webkit-limited-types bool YES
    plistbuddy Add :com.apple.developer.coremedia.allow-alternate-video-decoder-selection bool YES
    plistbuddy Add :com.apple.mediaremote.set-playback-state bool YES
    plistbuddy Add :com.apple.mediaremote.external-artwork-validation bool YES
    plistbuddy add :com.apple.mediaremote.ui-control bool YES
    plistbuddy Add :com.apple.private.allow-explicit-graphics-priority bool YES
    plistbuddy Add :com.apple.private.coremedia.extensions.audiorecording.allow bool YES
    plistbuddy Add :com.apple.private.mediaexperience.startrecordinginthebackground.allow bool YES
    plistbuddy Add :com.apple.private.mediaexperience.processassertionaudittokens.allow bool YES
    plistbuddy add :com.apple.private.mediaexperience.recordingWebsite.allow bool YES
    plistbuddy Add :com.apple.private.coremedia.pidinheritance.allow bool YES
    plistbuddy Add :com.apple.private.memorystatus bool YES
    plistbuddy Add :com.apple.private.memory.ownership_transfer bool YES
    plistbuddy Add :com.apple.private.network.socket-delegate bool YES
    plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES

    plistbuddy Add :com.apple.tcc.delegated-services array
    plistbuddy Add :com.apple.tcc.delegated-services:0 string kTCCServiceCamera
    plistbuddy Add :com.apple.tcc.delegated-services:1 string kTCCServiceMicrophone

    plistbuddy Add :com.apple.springboard.statusbarstyleoverrides bool YES
    plistbuddy Add :com.apple.springboard.statusbarstyleoverrides.coordinator array
    plistbuddy Add :com.apple.springboard.statusbarstyleoverrides.coordinator:0 string UIStatusBarStyleOverrideWebRTCAudioCapture
    plistbuddy Add :com.apple.springboard.statusbarstyleoverrides.coordinator:1 string UIStatusBarStyleOverrideWebRTCCapture

if [[ "${PRODUCT_NAME}" != GPUExtension ]]; then
    plistbuddy Add :com.apple.private.gpu-restricted bool YES
    plistbuddy Add :com.apple.private.pac.exception bool YES
    plistbuddy Add :com.apple.private.sandbox.profile string com.apple.WebKit.GPU
    plistbuddy Add :com.apple.private.coremedia.allow-fps-attachment bool YES
fi

    plistbuddy Add :com.apple.systemstatus.activityattribution bool YES
    plistbuddy Add :com.apple.security.exception.mach-lookup.global-name array
    plistbuddy Add :com.apple.security.exception.mach-lookup.global-name:0 string com.apple.systemstatus.activityattribution
    plistbuddy Add :com.apple.private.attribution.explicitly-assumed-identities array
    plistbuddy Add :com.apple.private.attribution.explicitly-assumed-identities:0:type string wildcard

    plistbuddy add :com.apple.coreaudio.allow-vorbis-decode bool YES

if [[ "${WK_PLATFORM_NAME}" == xros ]]; then
    plistbuddy Add :com.apple.surfboard.application-service-client bool YES
    plistbuddy Add :com.apple.surfboard.shared-simulation-connection-request bool YES
    plistbuddy Add :com.apple.surfboard.shared-simulation-memory-attribution bool YES
fi

    plistbuddy Add :com.apple.developer.hardened-process bool YES

    plistbuddy Add :com.apple.developer.kernel.extended-virtual-addressing bool YES
}

function ios_family_process_model_entitlements()
{
    plistbuddy Add :com.apple.QuartzCore.secure-mode bool YES
    plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
    plistbuddy add :com.apple.QuartzCore.webkit-limited-types bool YES
    plistbuddy Add :com.apple.private.memorystatus bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES
    plistbuddy Add :com.apple.private.sandbox.profile string com.apple.WebKit.Model
    plistbuddy Add :com.apple.private.pac.exception bool YES
    plistbuddy Add :com.apple.pac.shared_region_id string WebKitModel
    plistbuddy Add :com.apple.developer.hardened-process bool YES
}

function ios_family_process_adattributiond_entitlements()
{
    plistbuddy Add :com.apple.private.sandbox.profile string com.apple.WebKit.adattributiond
}

function ios_family_process_webpushd_entitlements()
{
    plistbuddy Add :com.apple.private.sandbox.profile string com.apple.WebKit.webpushd
    plistbuddy Add :aps-connection-initiate bool YES
    plistbuddy Add :com.apple.private.launchservices.allowopenwithanyhandler bool YES
    plistbuddy Add :com.apple.springboard.opensensitiveurl bool YES
    plistbuddy Add :com.apple.private.launchservices.canspecifysourceapplication bool YES
    plistbuddy Add :com.apple.usernotification.notificationschedulerproxy bool YES
    plistbuddy Add :com.apple.uikitservices.app.value-access bool YES
    plistbuddy Add :com.apple.private.usernotifications.app-management-domain.proxy string com.apple.WebKit.PushBundles
    plistbuddy Add :com.apple.frontboard.launchapplications bool YES
    plistbuddy Add :com.apple.private.security.storage.os_eligibility.readonly bool YES
    plistbuddy Add :com.apple.security.exception.files.absolute-path.read-only array
    plistbuddy Add :com.apple.security.exception.files.absolute-path.read-only:0 string /private/var/db/os_eligibility/eligibility.plist
}

function ios_family_process_network_entitlements()
{
    plistbuddy add :application-identifier string "${PRODUCT_BUNDLE_IDENTIFIER}"

    plistbuddy Add :com.apple.private.coreservices.canmaplsdatabase bool YES
    plistbuddy Add :com.apple.private.webkit.adattributiond bool YES
    plistbuddy Add :com.apple.private.webkit.webpush bool YES
    plistbuddy Add :com.apple.multitasking.systemappassertions bool YES
    plistbuddy Add :com.apple.payment.all-access bool YES
    plistbuddy Add :com.apple.private.accounts.bundleidspoofing bool YES
    plistbuddy Add :com.apple.private.ciphermld.allow bool YES
    plistbuddy Add :com.apple.private.dmd.policy bool YES
    plistbuddy Add :com.apple.private.memorystatus bool YES
    plistbuddy Add :com.apple.private.network.socket-delegate bool YES
    plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES

    plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token array
    plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token:0 string kTCCServiceWebKitIntelligentTrackingPrevention
    plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token:1 string kTCCServiceUserTracking

    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit

    plistbuddy Add :com.apple.private.appstored array
    plistbuddy Add :com.apple.private.appstored:0 string InstallWebAttribution

if [[ "${PRODUCT_NAME}" != NetworkingExtension ]]; then
    plistbuddy Add :com.apple.private.pac.exception bool YES
    plistbuddy Add :com.apple.private.sandbox.profile string com.apple.WebKit.Networking
fi
    plistbuddy Add :com.apple.symptom_analytics.configure bool YES

    plistbuddy Add :com.apple.private.assets.bypass-asset-types-check bool YES
    plistbuddy Add :com.apple.private.assets.accessible-asset-types array
    plistbuddy Add :com.apple.private.assets.accessible-asset-types:0 string com.apple.MobileAsset.WebContentRestrictions
    plistbuddy Add :com.apple.developer.hardened-process bool YES
}

rm -f "${WK_PROCESSED_XCENT_FILE}"
plistbuddy Clear dict

# Simulator entitlements should be added to one or more of:
#
#  - Resources/ios/XPCService-embedded-simulator.entitlements
#  - Shared/AuxiliaryProcessExtensions/GPUProcessExtension.entitlements
#  - Shared/AuxiliaryProcessExtensions/NetworkingProcessExtension.entitlements
#  - Shared/AuxiliaryProcessExtensions/WebContentProcessExtension.entitlements
if [[ "${WK_PLATFORM_NAME}" =~ .*simulator ]]
then
    exit 0
elif [[ "${WK_PLATFORM_NAME}" == macosx ]]
then
    [[ "${RC_XBS}" != YES ]] && plistbuddy Add :com.apple.security.get-task-allow bool YES

    if [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.Development ]]; then mac_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent ]]; then mac_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.CaptivePortal ]]; then mac_process_webcontent_captiveportal_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Networking ]]; then mac_process_network_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.GPU ]]; then mac_process_gpu_entitlements
    elif [[ "${PRODUCT_NAME}" == webpushd ]]; then mac_process_webpushd_entitlements
    elif [[ "${PRODUCT_NAME}" != adattributiond ]]; then echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
elif [[ "${WK_PLATFORM_NAME}" == maccatalyst || "${WK_PLATFORM_NAME}" == iosmac ]]
then
    [[ "${RC_XBS}" != YES && ( "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.Development ) ]] && plistbuddy Add :com.apple.security.get-task-allow bool YES

    if [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.Development ]]; then maccatalyst_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent ]]; then maccatalyst_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.CaptivePortal ]]; then maccatalyst_process_webcontent_captiveportal_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Networking ]]; then maccatalyst_process_network_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.GPU ]]; then maccatalyst_process_gpu_entitlements
    else echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
elif [[ "${WK_PLATFORM_NAME}" == iphoneos ||
        "${WK_PLATFORM_NAME}" == appletvos ||
        "${WK_PLATFORM_NAME}" == watchos ||
        "${WK_PLATFORM_NAME}" == xros ]]
then
    if [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.Development ]]; then true
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent ]]; then ios_family_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == WebContentExtension ]]; then ios_family_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.CaptivePortal ]]; then ios_family_process_webcontent_captiveportal_entitlements
    elif [[ "${PRODUCT_NAME}" == WebContentCaptivePortalExtension ]]; then ios_family_process_webcontent_captiveportal_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Networking ]]; then ios_family_process_network_entitlements
    elif [[ "${PRODUCT_NAME}" == NetworkingExtension ]]; then ios_family_process_network_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.GPU ]]; then ios_family_process_gpu_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Model ]]; then ios_family_process_model_entitlements
    elif [[ "${PRODUCT_NAME}" == GPUExtension ]]; then ios_family_process_gpu_entitlements
    elif [[ "${PRODUCT_NAME}" == adattributiond ]]; then
        ios_family_process_adattributiond_entitlements
    elif [[ "${PRODUCT_NAME}" == webpushd ]]; then
        ios_family_process_webpushd_entitlements
    else echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
else
    echo "Unsupported/unknown platform: ${WK_PLATFORM_NAME}"
fi

function process_additional_entitlements()
{
    local ENTITLEMENTS_SCRIPT=$1
    shift
    for PREFIX in "${@}"; do
        if [[ -f "${PREFIX}/${ENTITLEMENTS_SCRIPT}" ]]; then
            source "${PREFIX}/${ENTITLEMENTS_SCRIPT}"
            break
        fi
    done
}

ADDITIONAL_ENTITLEMENTS_SCRIPT=usr/local/include/WebKitAdditions/Scripts/process-additional-entitlements.sh
process_additional_entitlements "${ADDITIONAL_ENTITLEMENTS_SCRIPT}" "${BUILT_PRODUCTS_DIR}" "${SDKROOT}"

exit 0
