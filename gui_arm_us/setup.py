#!/usr/bin/env python
from distutils.core import setup
from catkin_pkg.python_setup import generate_distutils_setup
d = generate_distutils_setup(
    packages=['gui_arm_us'],
    package_dir={'': 'src'},
    scripts=['scripts/gui_arm_us'],
)
setup(**d)
