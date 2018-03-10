#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016, Olivier MATZ <zer0@droids-corp.org>

set -e

SEED=100
while [ ${SEED} -gt 0 ]; do
	CMD="./build/test --random-alloc-fail=1 --seed=${SEED} $*"
	${CMD} --log-level=0 || (
		echo "=== test failed, replay seed=${SEED} with logs ===" &&
		${CMD} --log-level=6 ||
		echo "=== test failed: ${CMD}" &&
		false
	)

	SEED=$((SEED-1)) && continue
done
