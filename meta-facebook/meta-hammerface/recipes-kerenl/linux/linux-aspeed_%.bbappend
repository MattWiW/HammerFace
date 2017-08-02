LINUX_VERSION_EXTENSION = "-hammerface"

COMPATIBLE_MACHINE = "hammerface"

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI += "file://defconfig \
           "

KERNEL_MODULE_AUTOLOAD += " \
"
