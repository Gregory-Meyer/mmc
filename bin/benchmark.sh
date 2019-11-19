#!/usr/bin/env sh

for DOCUMENT in $1/*; do
    BASENAME=$(basename -- ${DOCUMENT})
    hyperfine \
        "md ${DOCUMENT} ${BASENAME}.zlib" \
        "gzip -kc ${DOCUMENT} > ${BASENAME}.gz" \
        "mi ${BASENAME}.zlib ${BASENAME}.out" \
        "gunzip -kc ${BASENAME}.gz > ${BASENAME}.out" \
        "mlc ${DOCUMENT} ${BASENAME}.lz4" \
        "lz4 -f ${DOCUMENT} ${BASENAME}.lz4" \
        "mld ${BASENAME}.lz4 ${BASENAME}.out" \
        "unlz4 -f ${BASENAME}.lz4 ${BASENAME}.out" \
        --warmup 64 \
        --export-csv ${BASENAME}.csv
done
