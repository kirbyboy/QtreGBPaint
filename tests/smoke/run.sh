#!/usr/bin/env bash
# Build and run the SDL/ImGui smoke tests from tests/smoke/.
#
# Builds run in parallel (one link per smoke). Runs are serial because the
# smokes open SDL windows and warp the real pointer.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SMOKE_DIR="$ROOT_DIR/tests/smoke"
WORK_DIR="$ROOT_DIR/temp"
mkdir -p "$WORK_DIR"
JOBS="$(nproc)"
DO_BUILD=1
DO_RUN=1
NAMES=()

usage() {
	cat <<'EOF'
Usage:
  ./tests/smoke/run.sh                 build and run every smoke
  ./tests/smoke/run.sh circle buttons  build and run only those
  ./tests/smoke/run.sh --list          list available smoke names
  ./tests/smoke/run.sh --build-only    build without running
  ./tests/smoke/run.sh --no-build      run existing binaries
  ./tests/smoke/run.sh -j 4 circle     parallel build jobs (default: nproc)
  ./tests/smoke/run.sh --help

Environment:
  SMOKE_RETRIES=N                     retry failed smokes N times (CI uses 2)
EOF
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		--list)
			find "$SMOKE_DIR" -maxdepth 1 -name 'smoke_*.cpp' -printf '%f\n' \
				| sed 's/^smoke_//; s/\.cpp$//' | sort
			exit 0
			;;
		--build-only)
			DO_RUN=0
			shift
			;;
		--no-build)
			DO_BUILD=0
			shift
			;;
		-j|--jobs)
			if [[ $# -lt 2 ]]; then
				echo "missing value for $1" >&2
				exit 2
			fi
			JOBS="$2"
			shift 2
			;;
		-h|--help)
			usage
			exit 0
			;;
		-*)
			echo "unknown option: $1" >&2
			usage >&2
			exit 2
			;;
		*)
			NAMES+=("$1")
			shift
			;;
	esac
done

if [[ ${#NAMES[@]} -eq 0 ]]; then
	mapfile -t NAMES < <(
		find "$SMOKE_DIR" -maxdepth 1 -name 'smoke_*.cpp' -printf '%f\n' \
			| sed 's/^smoke_//; s/\.cpp$//' | sort
	)
fi

if [[ ${#NAMES[@]} -eq 0 ]]; then
	echo "no smoke sources found in $SMOKE_DIR" >&2
	exit 2
fi

for name in "${NAMES[@]}"; do
	if [[ ! -f "$SMOKE_DIR/smoke_${name}.cpp" ]]; then
		echo "missing source: $SMOKE_DIR/smoke_${name}.cpp" >&2
		exit 2
	fi
done

if [[ "$DO_BUILD" -eq 1 ]]; then
	first_object="$(find "$ROOT_DIR/obj" -name '*.o' ! -path '*/main.o' -print -quit 2>/dev/null || true)"
	if [[ -z "$first_object" ]]; then
		echo "no application objects in obj/; run make first" >&2
		exit 2
	fi

	mapfile -t OBJECTS < <(
		find "$ROOT_DIR/obj" -name '*.o' ! -path '*/main.o' | sort
	)

	build_one() {
		local name="$1"
		local log="$WORK_DIR/smoke_${name}.build.log"
		{
			g++ -std=c++17 \
				-I"$ROOT_DIR/inc" \
				-I"$ROOT_DIR/lib/imgui/src" \
				-I"$ROOT_DIR/lib/imgui/src/backends" \
				-I"$ROOT_DIR/third_party/ImGuiFileDialog" \
				$(sdl2-config --cflags) \
				"$SMOKE_DIR/smoke_${name}.cpp" \
				"${OBJECTS[@]}" \
				"$ROOT_DIR/third_party/ImGuiFileDialog/libImGuiFileDialog.a" \
				"$ROOT_DIR/lib/imgui/libimgui.a" \
				$(sdl2-config --libs) -lz -pthread \
				-o "$WORK_DIR/smoke_${name}"
		} >"$log" 2>&1
	}

	pids=()
	pending=()
	for name in "${NAMES[@]}"; do
		build_one "$name" &
		pids+=("$!")
		pending+=("$name")

		if (( ${#pids[@]} >= JOBS )); then
			if ! wait "${pids[0]}"; then
				echo "build failed: ${pending[0]}" >&2
				cat "$WORK_DIR/smoke_${pending[0]}.build.log" >&2
				exit 1
			fi
			rm -f "$WORK_DIR/smoke_${pending[0]}.build.log"
			pids=("${pids[@]:1}")
			pending=("${pending[@]:1}")
		fi
	done

	for i in "${!pids[@]}"; do
		if ! wait "${pids[$i]}"; then
			echo "build failed: ${pending[$i]}" >&2
			cat "$WORK_DIR/smoke_${pending[$i]}.build.log" >&2
			exit 1
		fi
		rm -f "$WORK_DIR/smoke_${pending[$i]}.build.log"
	done

	echo "built ${#NAMES[@]} smoke(s) with -j$JOBS"
fi

if [[ "$DO_RUN" -eq 1 ]]; then
	display="${DISPLAY:-:0}"
	failures=0

	for name in "${NAMES[@]}"; do
		binary="$WORK_DIR/smoke_${name}"
		if [[ ! -x "$binary" ]]; then
			echo "missing binary: $binary (remove --no-build)" >&2
			exit 2
		fi

		attempt=0
		while :; do
			if output=$(DISPLAY="$display" "$binary" 2>&1); then
				code=0
			else
				code=$?
			fi
			if (( code == 0 || attempt >= ${SMOKE_RETRIES:-0} )); then
				break
			fi
			attempt=$((attempt + 1))
			echo "retrying $name ($attempt/${SMOKE_RETRIES:-0})" >&2
			sleep 1
		done

		printf '%-22s exit=%d\n' "$name" "$code"
		if (( code != 0 )); then
			printf '%s\n' "$output" >&2
			failures=$((failures + 1))
		fi
	done

	if (( failures > 0 )); then
		echo "failed: $failures of ${#NAMES[@]} smoke(s)" >&2
		exit 1
	fi
	echo "all ${#NAMES[@]} smoke(s) passed"
fi
