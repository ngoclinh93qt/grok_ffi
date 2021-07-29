#!/bin/bash

# This script executes the install step when running under travis-ci

#if cygwin, check path
case ${MACHTYPE} in
	*cygwin*) GROK_CI_IS_CYGWIN=1;;
	*) ;;
esac

if [ "${GROK_CI_IS_CYGWIN:-}" == "1" ]; then
	# PATH is not yet set up
	export PATH=$PATH:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
fi

# Set-up some error handling
set -o nounset   ## set -u : exit the script if you try to use an uninitialised variable
set -o errexit   ## set -e : exit the script if any statement returns a non-true return value
set -o pipefail  ## Fail on error in pipe

function exit_handler ()
{
	local exit_code="$?"
	
	test ${exit_code} == 0 && return;

	echo -e "\nInstall failed !!!\nLast command at line ${BASH_LINENO}: ${BASH_COMMAND}";
	exit "${exit_code}"
}
trap exit_handler EXIT
trap exit ERR

if [ "${GROK_CI_SKIP_TESTS:-}" != "1" ]; then

	GROK_SOURCE_DIR=$(cd $(dirname $0)/../.. && pwd)

	# We need test data
	if [ "${TRAVIS_BRANCH:-}" != "" ]; then
		GROK_DATA_BRANCH=${TRAVIS_BRANCH}
	elif [ "${APPVEYOR_REPO_BRANCH:-}" != "" ]; then
		GROK_DATA_BRANCH=${APPVEYOR_REPO_BRANCH}
	else
		GROK_DATA_BRANCH=$(git -C ${GROK_SOURCE_DIR} branch | grep '*' | tr -d '*[[:blank:]]') #default to same branch as we're setting up
	fi
	GROK_DATA_HAS_BRANCH=$(git ls-remote --heads git://github.com/GrokImageCompression/grok-test-data.git ${GROK_DATA_BRANCH} | wc -l)
	if [ ${GROK_DATA_HAS_BRANCH} -eq 0 ]; then
		GROK_DATA_BRANCH=master #default to master
	fi
	if [ "${TRAVIS_OS_NAME:-}" == "osx" ] || uname -s | grep -i Darwin &> /dev/null; then
		pip install --user six
	fi
	echo "Cloning grok-test-data from ${GROK_DATA_BRANCH} branch"
	git clone --depth=1 --branch=${GROK_DATA_BRANCH} git://github.com/GrokImageCompression/grok-test-data.git data


	# We need jpylyzer for the test suite
    JPYLYZER_VERSION="2.0.0"    
	echo "Retrieving jpylyzer"
	if [ "${APPVEYOR:-}" == "True" ]; then
		wget -q https://github.com/openpreserve/jpylyzer/releases/download/2.0.0/jpylyzer_${JPYLYZER_VERSION}_win32.zip
		cmake -E tar -xzf jpylyzer_${JPYLYZER_VERSION}_win32.zip
	else
		wget -qO - https://github.com/openpreserve/jpylyzer/archive/${JPYLYZER_VERSION}.tar.gz | tar -xz
		mv jpylyzer-${JPYLYZER_VERSION} jpylyzer
		chmod +x jpylyzer/cli.py
	fi

	# When GROK_NONCOMMERCIAL=1, kakadu trial binaries are used for testing. Here's the copyright notice from kakadu:
	# Copyright is owned by NewSouth Innovations Pty Limited, commercial arm of the UNSW Australia in Sydney.
	# You are free to trial these executables and even to re-distribute them, 
	# so long as such use or re-distribution is accompanied with this copyright notice and is not for commercial gain.
	# Note: Binaries can only be used for non-commercial purposes.
	if [ "${GROK_NONCOMMERCIAL:-}" == "1" ]; then
		if [ "${TRAVIS_OS_NAME:-}" == "linux" ] || uname -s | grep -i Linux &> /dev/null; then
			echo "Retrieving Kakadu"
			wget -q http://kakadusoftware.com/wp-content/uploads/KDU805_Demo_Apps_for_Linux-x86-64_200602.zip
			cmake -E tar -xzf KDU805_Demo_Apps_for_Linux-x86-64_200602.zip
			mv KDU805_Demo_Apps_for_Linux-x86-64_200602 kdu
		elif [ "${TRAVIS_OS_NAME:-}" == "osx" ] || uname -s | grep -i Darwin &> /dev/null; then
			echo "Retrieving Kakadu"
			wget -q http://kakadusoftware.com/wp-content/uploads/KDU805_Demo_Apps_for_MacOS_200602.dmg_.zip
                        7z x KDU805_Demo_Apps_for_MacOS_200602.dmg_.zip
                        7z x KDU805_Demo_Apps_for_MacOS_200602.dmg
                        7z x KDU805_Demo_Apps_for_MacOS_200602/KDU805_Demo_Apps_for_MacOS_200602.pkg
                        7z x Payload~
                        mv Library kdu
			cd kdu
                        cp Kakadu/8.0.5/bin/kdu_expand .
                        cp Kakadu/8.0.5/bin/kdu_compress .
                        cp Kakadu/8.0.5/lib/libkdu_v80R.dylib .
                        chmod +x kdu_expand
                        chmod +x kdu_compress
                        install_name_tool -id ${PWD}/libkdu_v80R.dylib libkdu_v80R.dylib 
			install_name_tool -change /usr/local/lib/libkdu_v80R.dylib ${PWD}/libkdu_v80R.dylib kdu_compress
			install_name_tool -change /usr/local/lib/libkdu_v80R.dylib ${PWD}/libkdu_v80R.dylib kdu_expand
		elif [ "${APPVEYOR:-}" == "True" ]; then
			echo "Retrieving Kakadu"
			wget -q http://kakadusoftware.com/wp-content/uploads/KDU805_Demo_Apps_for_Win64_200602.msi_.zip
			cmake -E tar -xzf KDU805_Demo_Apps_for_Win64_200602.msi_.zip
			msiexec /i KDU805_Demo_Apps_for_Win64_200602.msi /quiet /qn /norestart
			if [ -d "C:/Program Files/Kakadu" ]; then
				cp -r "C:/Program Files/Kakadu/Kakadu Demo-Apps" ./kdu
			else
				cp -r "C:/Program Files (x86)/Kakadu/Kakadu Demo-Apps" ./kdu
			fi
		fi
	fi
fi

if [ "${GROK_CI_CHECK_STYLE:-}" == "1" ]; then
    pip install --user autopep8
fi
