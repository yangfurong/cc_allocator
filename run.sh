#!/bin/bash

perf stat -e r530324:uh,r530124:uh,LLC-loads:uh,LLC-load-misses:uh,LLC-stores:uh,LLC-store-misses:uh ./miss
