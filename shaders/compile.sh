#!/bin/bash
glslangValidator -V shader.vert -o shader.vert.spv
glslangValidator -V shader.frag -o shader.frag.spv
glslangValidator -V depth.vert -o depth.vert.spv
glslangValidator -V depth.frag -o depth.frag.spv
