u80211_drv_include = include_directories('../include')

subdir('rtl8188eu')

u80211_drv_library = static_library(
  'u80211_drv',
  'u80211_drv.c',
  'string.c',
  include_directories: u80211_drv_include,
  dependencies: u80211_dep,
)

u80211_drv_dep = declare_dependency(
  link_with: [u80211_drv_library, rtl8188eu_library],
  include_directories: u80211_drv_include,
  dependencies: u80211_dep,
)
