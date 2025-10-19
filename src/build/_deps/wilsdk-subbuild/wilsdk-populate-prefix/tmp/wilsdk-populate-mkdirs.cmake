# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Work/GIT/misc/neuron-guitar/src/build/_deps/wilsdk-src")
  file(MAKE_DIRECTORY "C:/Work/GIT/misc/neuron-guitar/src/build/_deps/wilsdk-src")
endif()
file(MAKE_DIRECTORY
  "C:/Work/GIT/misc/neuron-guitar/src/build/_deps/wilsdk-build"
  "C:/Work/GIT/misc/neuron-guitar/src/build/_deps/wilsdk-subbuild/wilsdk-populate-prefix"
  "C:/Work/GIT/misc/neuron-guitar/src/build/_deps/wilsdk-subbuild/wilsdk-populate-prefix/tmp"
  "C:/Work/GIT/misc/neuron-guitar/src/build/_deps/wilsdk-subbuild/wilsdk-populate-prefix/src/wilsdk-populate-stamp"
  "C:/Work/GIT/misc/neuron-guitar/src/build/_deps/wilsdk-subbuild/wilsdk-populate-prefix/src"
  "C:/Work/GIT/misc/neuron-guitar/src/build/_deps/wilsdk-subbuild/wilsdk-populate-prefix/src/wilsdk-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Work/GIT/misc/neuron-guitar/src/build/_deps/wilsdk-subbuild/wilsdk-populate-prefix/src/wilsdk-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Work/GIT/misc/neuron-guitar/src/build/_deps/wilsdk-subbuild/wilsdk-populate-prefix/src/wilsdk-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
