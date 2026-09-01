project(
  'u80211_drv',
  'c',
  version: '0.1.0',
  meson_version: '>=0.56.0',
  default_options: [
    'c_std=c11',
    'warning_level=3',
  ],
)

u80211_dep = dependency(
  'u80211',
  fallback: ['u80211', 'u80211_dep'],
)

subdir('src')

subdir('tests')
