CONFIG ?= clock.config.toml
PORT ?=
PIO ?= uv run pio
CJSON_DIR ?= $(HOME)/.platformio/packages/framework-espidf/components/json/cJSON
PORT_ARG = $(if $(PORT),--port "$(PORT)",)

.PHONY: help validate build deploy monitor ports clean test test-network test-weather lint preview verify-build
help:
	@printf '%s\n' \
	  'make validate CONFIG=clock.config.toml' \
	  'make build CONFIG=clock.config.toml' \
	  'make deploy CONFIG=clock.config.toml PORT=/dev/cu.usbmodem...' \
	  'make ports | monitor PORT=... | clean | test | lint | preview | verify-build'
validate:
	uv run python -m tools.clockctl validate "$(CONFIG)"
build:
	uv run python -m tools.clockctl build "$(CONFIG)"
deploy:
	uv run python -m tools.clockctl deploy "$(CONFIG)" $(PORT_ARG)
monitor:
	$(PIO) device monitor $(PORT_ARG) --baud 115200
ports:
	$(PIO) device list
clean:
	CLOCK_CONFIG="$(abspath $(CONFIG))" $(PIO) run -t clean
test:
	uv run pytest
	$(MAKE) test-network
	$(MAKE) test-weather
test-network:
	uv run python -m tools.clockctl generate clock.config.example.toml .pio/generated/clock_config.h
	mkdir -p .pio/native
	$(CC) -std=c11 -D_POSIX_C_SOURCE=200809L -Dtime=simulated_time -Wall -Wextra -Werror \
	  -Itests/native/include -Isrc -I.pio/generated \
	  tests/native/network_test.c src/network_time.c -o .pio/native/network_test
	.pio/native/network_test
test-weather:
	mkdir -p .pio/native
	$(CC) -std=c11 -Wall -Wextra -Werror -Isrc -I"$(CJSON_DIR)" \
	  tests/native/weather_test.c src/clock_weather_parse.c "$(CJSON_DIR)/cJSON.c" \
	  -lm -o .pio/native/weather_test
	.pio/native/weather_test
lint:
	uv run ruff check tools/clockctl.py tools/config.py tools/timezone.py tools/verify_build.py tests
preview:
	$(PIO) pkg exec -p tool-cmake -c "cmake -S tools/preview -B .pio/preview"
	$(PIO) pkg exec -p tool-cmake -c "cmake --build .pio/preview --target clock_preview -j 8"
	mkdir -p .pio/preview/frames
	cd .pio/preview/frames && ../clock_preview
verify-build:
	uv run python -m tools.verify_build
