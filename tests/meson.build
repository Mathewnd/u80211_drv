libusb_dep = dependency('libusb-1.0')
threads_dep = dependency('threads')

test_hardware = executable(
  'test_hardware',
  'test_hardware.c',
  'kernel_interface.c',
  dependencies: [u80211_drv_dep, libusb_dep, threads_dep],
)

test(
  'test_hardware',
  test_hardware,
  suite: 'integration',
  workdir: meson.project_source_root(),
)
