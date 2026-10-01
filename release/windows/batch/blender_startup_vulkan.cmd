@echo off
echo Starting Bmax with the Vulkan backend

"%~dp0\blender" --gpu-backend vulkan
