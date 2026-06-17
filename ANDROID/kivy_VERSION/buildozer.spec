[app]
title = Prismalux
package.name = prismalux
package.domain = org.prismalux
source.dir = .
source.include_exts = py,png,jpg,kv,atlas,json
source.exclude_dirs = .buildozer, tests, __pycache__, .git, res
version = 1.1

# pyttsx3 e sounddevice NON hanno recipe p4a — esclusi intenzionalmente
requirements = python3,kivy==2.3.0,requests,plyer,android

orientation = portrait
fullscreen = 0

android.permissions = INTERNET,RECORD_AUDIO,READ_EXTERNAL_STORAGE,WRITE_EXTERNAL_STORAGE
android.api = 33
android.minapi = 26
android.ndk = 25b
android.sdk = 33
android.archs = arm64-v8a
android.allow_backup = True
android.release_artifact = apk

# HTTP cleartext verso server LAN locale
android.extra_manifest_application_arguments = %(source.dir)s/android_manifest_app_args.txt
android.add_resources = %(source.dir)s/res/xml:xml

[buildozer]
log_level = 2
warn_on_root = 1
