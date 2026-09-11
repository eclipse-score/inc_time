#!/usr/bin/env bash
# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************
#
# Runs unit + component tests with code coverage and generates HTML +
# Cobertura XML reports.
#
# Prerequisites (install once):
#   sudo apt-get install -y lcov
#   pipx install lcov-cobertura
#
# Usage:
#   .github/tools/coverage.sh [<bazel-target>] [--config <bazel-config>] [--output-dir <dir>]
#
# Options:
#   <bazel-target>            Bazel target to collect coverage for (default: //score/...)
#   --config <bazel-config>   Bazel config to use (default: time-x86_64-linux)
#   --output-dir <dir>        Directory for generated reports (default: cpp_coverage)

set -euo pipefail

OUTPUT_DIR="cpp_coverage"
BAZEL_CONFIG="time-x86_64-linux"
BAZEL_TARGET="${1:-//score/...}"

# Consume the target argument if it was provided positionally
[[ $# -gt 0 && "$1" != --* ]] && shift

while [[ $# -gt 0 ]]; do
    case "$1" in
        --config)
            BAZEL_CONFIG="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --target)
            BAZEL_TARGET="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

echo "==> Running tests with coverage..."
bazel coverage --config="${BAZEL_CONFIG}" -- "${BAZEL_TARGET}"

OUTPUT_PATH="$(bazel info output_path)"
EXEC_ROOT="$(bazel info execution_root)"
DAT_FILE="${OUTPUT_PATH}/_coverage/_coverage_report.dat"

echo "==> Generating HTML report in '${OUTPUT_DIR}'..."
genhtml "${DAT_FILE}" \
    --output-directory="${OUTPUT_DIR}" \
    --show-details \
    --source-directory="${EXEC_ROOT}" \
    --legend \
    --function-coverage \
    --branch-coverage

echo "==> Generating Cobertura XML report at '${OUTPUT_DIR}/coverage.xml'..."
lcov_cobertura "${DAT_FILE}" \
    --base-dir "${EXEC_ROOT}" \
    --output "${OUTPUT_DIR}/coverage.xml"

echo ""
echo "Coverage reports written to '${OUTPUT_DIR}/'."
echo "  HTML:      ${OUTPUT_DIR}/index.html"
echo "  Cobertura: ${OUTPUT_DIR}/coverage.xml"
