# Local Russian stress marks

Russian text in incoming and outgoing chat bubbles and media captions is marked
locally. Right-click a marked message and choose **View original**; choose
**Show stress marks** to restore the marked display. The choice lasts while that
message is loaded. Sending, forwarding, editing, storage, and the original
message text continue to use the original content.

The built Apple Silicon Debug app is `out/Debug/Telegram.app` and requires
macOS 14 or later. The build, both regression suites, signature verification,
and startup/shutdown with an isolated profile have been checked. Live chat
behavior has not been exercised with a signed-in account.

Rich-message paragraphs, lists, tables, and captions are marked too. For a
selected excerpt from a rich message, switch to **View original** before
quoting it; Telegram's native quote model refers to the original rich document.

Inference runs on a serial background queue. A message initially appears with
its original text and refreshes when inference completes. Existing accents,
whitespace, emoji, formatting, and link destinations are preserved. Code,
URLs, mentions, hashtags, and words containing non-Russian letters are excluded.
Predictions are those of stresspp, including its confidence threshold; uncertain
words can remain unmarked. No text is sent to a stress-marking service.

## Build on Apple Silicon

Follow [the upstream macOS instructions](building-mac.md) to recursively clone
the repository and prepare dependencies:

```sh
./Telegram/build/prepare/mac.sh skip-release
bash tools/prepare_stress_macos.sh
```

The second command downloads Microsoft's official ONNX Runtime 1.22.0 Apple
archive, verifies its pinned SHA-256, and extracts its ARM64 static library.
It also retains Microsoft's license under `.stress-build/onnxruntime-static`.
The runtime and stresspp are linked into the executable; there is no stresspp
subprocess or ONNX Runtime dylib to install.

The stresspp source files are compiled directly from `~/Projects/stresspp`.
The embedded model and dictionary are linked into the binary. The integration
supplies a session initializer with an explicit destruction order for the
static runtime, without modifying the external stresspp checkout.

With `API_ID` and `API_HASH` exported in the shell:

```sh
./Telegram/configure.sh \
  -C "$PWD/Telegram/cmake/stress_api.cmake" \
  -D DESKTOP_APP_MAC_ARCH=arm64 \
  -D CMAKE_OSX_ARCHITECTURES=arm64 \
  -D CMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -D TDESKTOP_STRESS_TESTS=ON \
  -D DESKTOP_APP_DISABLE_AUTOUPDATE=ON
cmake --build out --config Debug --target Telegram
cmake --build out --config Debug --target stress_marking_tests stresspp_regression_tests
ctest --test-dir out -C Debug --output-on-failure -R '^stress'
```

The API cache initializer reads the environment without printing credential
values. As with upstream builds, credentials are stored in the local CMake
cache and compiled into the client.

`TDESKTOP_STRESSPP_ROOT` and `TDESKTOP_ONNXRUNTIME_ROOT` can override the source
and static-library directories. The latter must contain `include/` and
`lib/libonnxruntime.a`; configuration rejects a dynamic library in its place.
Automatic upstream updates are disabled for this custom client so an official
update does not replace the integration.

Check the final executable with `otool -L`; it must not list an ONNX Runtime
dylib. Apple system frameworks remain normal system dependencies.
