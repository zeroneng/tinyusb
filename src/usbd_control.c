/* usbd_control.c — compatibility object for libDaisy/TinyUSB builds
 *
 * Some libDaisy makefiles still depend on build/usbd_control.o even when the
 * bundled TinyUSB version has the control-transfer implementation folded into
 * src/device/usbd.c and has no separate src/device/usbd_control.c.
 *
 * Keep this file intentionally empty. Do not include TinyUSB's usbd_control.c
 * here unless your TinyUSB tree actually provides that file and you remove this
 * placeholder from the build.
 */
