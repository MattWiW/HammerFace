
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI += "file://hammerface.conf \
           "

do_install_append() {
    install -d ${D}${sysconfdir}/sensors.d
    install -m 644 ../hammerface.conf ${D}${sysconfdir}/sensors.d/hammerface.conf
}
