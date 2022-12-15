#!/usr/bin/env bash

set -e

# GNU prefix command for mac os support (gsed, gsplit)
GP=
if [[ "$OSTYPE" =~ darwin* ]]; then
  GP=g
fi
# Mac compatibility support for find
GF=
if [[ "$OSTYPE" =~ darwin* ]]; then
  GF=" -E"
fi


for f in $(find ${GF} src -type f -regex '.*\.(h|cpp)$'); do
  #echo $f
  subdir=$(echo ${f} | cut -d/ -f2)
  ext=$(echo ${f} | rev | cut -d. -f1 | rev)
  if [[ ${ext} = "cpp" ]]; then
    if [[ ${subdir} =~ (3d) ]]; then
      # no need to check for inclusions of sources in libs
      continue
    fi
  fi
  if [[ ${ext} = "h" ]]; then
    if [[ ${subdir} =~ (app) ]]; then
      # no need to check for inclusions of headers in app
      continue
    fi
  fi
  include_path=$(gsed "s|src/${subdir}/||; s/\.(h|cpp)$//" <<< ${f})
  if [[ $(ag -c "${include_path}" src/${subdir}/CMakeLists.txt) -lt 1 ]]; then
    if [[ ${ext} = "h" ]]; then
        echo "!!! $f: not included in CMakeLists"
      return_code=1
    fi
  # else
  #   header_file=$(echo {$f} | sed 's/\.cpp$/.h/')
  #   if [[ ! -f ${header_file} ]]; then
  #     echo "!!! $f: header does not exit"
  #     return_code=1
  #   fi
  fi
done


exit ${return_code}



# make private:
FILE=src/core/providers/ogr/qgsgeopackagerasterwriter.h
RELPATH=$(echo ${FILE} | sed 's|src/\w+/||')
FILENAME=$(echo ${FILE} | rev | cut -d/ -f1 | rev)
mv ${FILE} $(echo ${FILE} | sed 's/\.h/_p.h/')
NEWNAME=$(echo ${FILENAME} | sed 's/\.h/_p.h/')
ag -c ${FILENAME} src | cut -d: -f1 | xargs ${GP}sed -i "s/${FILENAME}/${NEWNAME}/"
