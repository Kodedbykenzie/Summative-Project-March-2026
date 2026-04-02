from setuptools import setup, Extension

module = Extension(
    "vibration",
    sources=["vibration.c"],
)

setup(
    name="vibration",
    version="1.0",
    description="C extension for vibration statistics (course demo)",
    ext_modules=[module],
)