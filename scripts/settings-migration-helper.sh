#!/usr/bin/env bash

set -e

MODULE_CODE=Core
MODULE=${MODULE_CODE,,}

ADDSETTING_REG_H_Core=179
ADDSETTING_REG_CPP_Core=137
INCLUDE_REG_CPP_Core=28

ADDSETTING_REG_H_Gui=54
ADDSETTING_REG_CPP_Gui=28
INCLUDE_REG_CPP_Gui=21

if [[ $1 == x ]]; then
  set -x
fi

# GNU prefix command for bsd/mac os support (gsed, gsplit)
GP=
if [[ "$OSTYPE" == *bsd* ]] || [[ "$OSTYPE" =~ darwin* ]]; then
  GP=g
fi


DATA_FILE=scripts/settings-migration.dat

touch $DATA_FILE


ALREADY_PROCESSED="-"

declare -a SETTING_TYPES=()
SETTING_TYPES[0]="EnumFlag::flagValue:setFlagValue"
SETTING_TYPES[1]="EnumFlag::enumValue:setEnumValue"
SETTING_TYPES[2]="Double:.to(Double|Float)\(\):"
SETTING_TYPES[3]="Integer:.toInt\(\):"
SETTING_TYPES[4]="String:.toString\(\):"
SETTING_TYPES[5]="Bool:.toBool\(\):"
SETTING_TYPES[6]="StringList:.toStringList\(\):"

declare -a HEADER_OFFSET=()
HEADER_OFFSET[0]="qgsrasterlayer.h:6"
HEADER_OFFSET[0]="pal.h:18"

declare -a ENUM_DEF2TYPE=()
# def value in code ! Enum type ! Default value if first is def override
ENUM_DEF2TYPE[0]="mSimplifyMethod.simplifyHints()!QgsVectorSimplifyMethod::SimplifyHint!QgsVectorSimplifyMethod::SimplifyHint::NoSimplification"
ENUM_DEF2TYPE[1]="mSimplifyMethod.simplifyAlgorithm()!QgsVectorSimplifyMethod::SimplifyAlgorithm!QgsVectorSimplifyMethod::SimplifyAlgorithm::Distance"

for _SETTING_TYPE in "${SETTING_TYPES[@]}"; do
  SETTING_TYPE=$(echo ${_SETTING_TYPE} | cut -d: -f1)
  SETTING_TYPE_TO=$(echo ${_SETTING_TYPE} | cut -d: -f2)
  VALUE_GET=$(echo ${_SETTING_TYPE} | cut -d: -f3)
  VALUE_SET=$(echo ${_SETTING_TYPE} | cut -d: -f4)
  VALUE_GET=${VALUE_GET:-value}
  VALUE_SET=${VALUE_SET:-setValue}
  if [[ $SETTING_TYPE == EnumFlag ]]; then
    INCLUDE_FILE="qgssettingsentryimpl.h"
  else
    INCLUDE_FILE="qgssettingsentryimpl.h"
  fi
  while read -u 3 -r LINE; do
    # echo "****"
    echo $LINE
    FILE=$(echo "${LINE}" | cut -d: -f1)
    CODE=$(echo "${LINE}" | cut -d: -f2,3,4,5,6)
    SETTING_DEFAULT=$(echo "${CODE}" | ${GP}sed -r "s/^.*${VALUE_GET}\( QStringLiteral\( \"([^ ]+)\" \)(, ([^ ]+))?(, QgsSettings::(Section::)?(\w+))? \).*$/\1!\3!\6/")
    SETTING=$(echo "${SETTING_DEFAULT}" | cut -d! -f1)
    DEFAULT=$(echo "${SETTING_DEFAULT}" | cut -d! -f2)
    SECTION=$(echo "${SETTING_DEFAULT}" | cut -d! -f3 | ${GP}sed 's/NoSection//')
    PREFIX_STR=$(echo "$SETTING" |  ${GP}sed -r 's|(/?%[0-9]+/.*)?/?[^/]+$||; s|^/||')
    KEY=$(echo "$SETTING" | ${GP}sed "s|${PREFIX_STR}/||")

    if [[ "${ALREADY_PROCESSED}" =~ -${SETTING}- ]]; then
      echo "$SETTING already processed"
    else
      ALREADY_PROCESSED="-${SETTING}${ALREADY_PROCESSED}"

      VALUE_DEFAULT_OVERRIDE_METHOD="value"
      VALUE_DEFAULT_OVERRIDE_VALUE=
      ENUM_SPEC=

      if [[ $SETTING_TYPE == EnumFlag ]]; then
        for dt in "${ENUM_DEF2TYPE[@]}"; do
          if [[ $(echo "${dt}" | cut -d! -f1) == "${DEFAULT}" ]]; then
            ENUM_SPEC="<$(echo "${dt}" | cut -d! -f2)>"
            NEW_DEFAULT="$(echo "${dt}" | cut -d! -f3)"
            if [[ -n ${NEW_DEFAULT} ]]; then
              VALUE_DEFAULT_OVERRIDE_METHOD="valueWithDefaultOverride"
              VALUE_DEFAULT_OVERRIDE_VALUE=${DEFAULT}
              DEFAULT=${NEW_DEFAULT}
            fi
            break
          fi
        done
      fi

      if [[ ${DEFAULT} =~ ^m ]]; then
        VALUE_DEFAULT_OVERRIDE_METHOD="valueWithDefaultOverride"
        VALUE_DEFAULT_OVERRIDE_VALUE=${DEFAULT}
        DEFAULT=
      fi
      if [[ -z ${DEFAULT} || ${DEFAULT} == "QVariant()" ]]; then
        if [[ ${SETTING_TYPE} == "StringList" ]]; then
           DEFAULT="QStringList()"
        fi
        if [[ ${SETTING_TYPE} == "String" ]]; then
           DEFAULT="QString()"
        fi
        if [[ ${SETTING_TYPE} == "Double" ]]; then
           DEFAULT="0.0"
        fi
        if [[ ${SETTING_TYPE} == "Integer" ]]; then
           DEFAULT="0"
        fi
        if [[ ${SETTING_TYPE} == "Bool" ]]; then
           DEFAULT="false"
        fi
      fi
      if [[ ${SETTING_TYPE} == "Integer" ]]; then
        DEFAULT=$(echo "${DEFAULT}" | ${GP}sed 's/"//g')
      fi

      #SETTING_VAR_NAME=$(echo "${KEY}" | ${GP}sed -r 's/^(.)/\U\1/; s/-')
      SETTING_VAR_NAME=${KEY}
      # PREPEND LAST PREFIX PART
      if [[ ! ${PREFIX_STR} =~ ^(UI|qgis)$ ]]; then
        SETTING_VAR_NAME=$(echo "${PREFIX_STR}" | ${GP}sed 's|^/?(\w+/)+||g'| ${GP}sed -r 's/^(.)/\U\1/')$(echo "${SETTING_VAR_NAME}" | ${GP}sed -r 's/^(.)/\U\1/')
      fi
      SETTING_VAR_NAME="settings"$(echo "${SETTING_VAR_NAME}" | ${GP}sed -r 's/^(.*)$/_\1/; s|/|_|g; s/__/_/; s/(_|-)([a-z])/\1\U\2/g; s/(-|_)//g; s/^_//;')

      PREFIX_STR=$(echo "${SECTION,,}_${PREFIX_STR}" | ${GP}sed 's/^_//')
      PREFIX_VAR=$(echo "${PREFIX_STR}" | ${GP}sed -r 's/^(.*)$/_\1/; s|/|_|g; s/__/_/; s/(_|-)([a-z])/\1\U\2/g; s/-//g; s/^_//;' | ${GP}sed -r 's/^(.*)$/\U\1/')

      if [[ -z $PREFIX_STR ]]; then
        PREFIX_VAR_COMPLETE="QgsSettings::Prefix::NO_PREFIX"
        PREFIX_VAR="NO_PREFIX"
      else
        PREFIX_VAR_COMPLETE="QgsSettings::Prefix::${PREFIX_VAR}"
      fi

      # for regexes
      SETTING_ESC=$(echo "${SETTING}" | ${GP}sed 's|/|.|g')


      HEADER=$(echo ${FILE} | ${GP}sed 's|\.cpp$|.h|')
      BASECLASS=$(basename ${HEADER} | ${GP}sed 's/\.h$//')

      if [[ ! ${HEADER} =~ \.h$ ]]; then
        echo "wrong header: $HEADER"
        exit 1
      fi

      # echo "$HEADER $BASECLASS"

      HAS_CLASS=$(git grep -i -c "^ *class ${MODULE}_EXPORT ${BASECLASS}\b" ${HEADER} | tail -1 | cut -d: -f2)
      CLASS_DEF=$(git grep -i "^ *class ${MODULE}_EXPORT ${BASECLASS}\b" ${HEADER} | tail -1 | cut -d: -f2 | ${GP}sed -r "s/^ *class ${MODULE}_EXPORT (${BASECLASS}).*$/\1/i")

      echo -e "   FILE: $FILE\n   SETTING_TYPE: $SETTING_TYPE\n   SETTING: $SETTING\n   DEFAULT: $DEFAULT\n   KEY: $KEY\n   PREFIX_STR: $PREFIX_STR\n   PREFIX_VAR: $PREFIX_VAR\n   SECTION: $SECTION\n   CLASS_DEF: $CLASS_DEF\n   HAS_CLASS: $HAS_CLASS\n   SETTING_VAR_NAME: $SETTING_VAR_NAME\n   HEADER: $HEADER\n   ENUM_SPEC: $ENUM_SPEC"

      MODE=0
      while read C_LINE; do
        C_SETTING=$(echo "$C_LINE" | cut -d: -f1)
        C_ACTION=$(echo "$C_LINE" | cut -d: -f2)
        if [[ "$C_SETTING" == "$SETTING" ]]; then
          echo "setting already in config: ${C_ACTION}"
          if [[ ${C_ACTION} == "class" ]]; then
            MODE="class"
            MODULE_REGISTRY=${MODULE_CODE}
          else
            MODE="registry"
            MODULE_REGISTRY="${C_ACTION}"
          fi
          break
        fi
      done<${DATA_FILE}

      if [[ "${MODE}" == 0 ]]; then
        echo "***"
        echo -e "Add ${SETTING}" to
        echo -e "  c) class ${CLASS_DEF}"
        echo -e "  r) registry ${MODULE_CODE}"
        echo -e "  o) registry CORE"
        echo -e "  s) skip and continue"
        echo -e "  e) exit"

        while read -n 1 n; do
          echo ""0
          case $n in
              c)
                echo -e "add to class"
                MODE=class
                # https://unix.stackexchange.com/a/533708/238014
                #${GP}sed -e "/^ *class ${MODULE}_EXPORT ${BASECLASS}/{:a; N; /\n *public:/!ba; a\XXXX" -e '}' ${HEADER}
                break
                ;;
              r)
                echo -e "add to registry ${MODULE_CODE}"
                MODE=registry
                MODULE_REGISTRY=${MODULE_CODE}
                break
                ;;
              o)
                echo -e "add to registry CORE"
                MODE=registry
                MODULE_REGISTRY=Core
                break
                ;;
              g)
                echo -e "add to registry GUI"
                MODE=registry
                MODULE_REGISTRY=Gui
                break
                ;;
              s)
                echo -e "skip"
                MODE=SKIP
                break
                ;;
              e)
                exit 1
                ;;
              *) invalid option;;
          esac
        done
      fi

      MODULE_REG_CPP=src/${MODULE_REGISTRY,,}/settings/qgssettingsregistry${MODULE_REGISTRY,,}.cpp
      MODULE_REG_H=src/${MODULE_REGISTRY,,}/settings/qgssettingsregistry${MODULE_REGISTRY,,}.h

      if [[ ${MODE} != skip ]]; then
        echo "$SETTING:$MODE" >> ${DATA_FILE}
        sort -u -o ${DATA_FILE} ${DATA_FILE}

        if [[ ${MODE} == class ]]; then
          SETTING_VAR_NAME_COMPLETE="${CLASS_DEF}::${SETTING_VAR_NAME}"
          SETTING_VAR_NAME_CLASS="${SETTING_VAR_NAME}"
          SETTING_VAR_NAME_REGISTRY="${CLASS_DEF}::${SETTING_VAR_NAME}"
          CLASS_FOUND=0
          shopt -s nocasematch
          IFS=''
          INCLUDE_IMPL_COUNT=$(git grep -c "#include \"${INCLUDE_FILE}\"" ${HEADER} | cut -d: -f2)
          if [[ -z $INCLUDE_IMPL_COUNT ]]; then
            OFFSET=20
            for hf in "${HEADER_OFFSET[@]}"; do
              if [[ $(echo "${hf}" | cut -d: -f1) == $(basename ${HEADER}) ]]; then
                OFFSET=$((${OFFSET}+$(echo "${hf}" | cut -d: -f2)))
                break
              fi
            done
            ${GP}sed -i "${OFFSET} i #include \"${INCLUDE_FILE}\"" ${HEADER}
          fi
          LINE_NUMBER=0
          while read F_LINE; do
            ((LINE_NUMBER+=1))
            if [[ ${CLASS_FOUND} == 0 ]]; then
              #echo '[[ "'${F_LINE//\"/\\\"g}'" =~ class\ '${MODULE}'_EXPORT\ '${BASECLASS}' ]]'
              if eval '[[ "'${F_LINE//\"/\\\"g}'" =~ class\ '${MODULE}'_EXPORT\ '${BASECLASS}' ]]'; then
                echo "CLASS_FOUND"
                CLASS_FOUND=1
              fi
            else
              if [[ "${F_LINE}" =~ ^\ *public:$ ]]; then
                echo "public found"
                ${GP}sed -i "$(( $LINE_NUMBER+2 )) i \ \ \ \ static const inline QgsSettingsEntry${SETTING_TYPE}${ENUM_SPEC} ${SETTING_VAR_NAME} = QgsSettingsEntry${SETTING_TYPE}${ENUM_SPEC}( QStringLiteral( \"${KEY}\" ), ${PREFIX_VAR_COMPLETE}, ${DEFAULT} ) SIP_SKIP;" ${HEADER}
                break
              fi
            fi
          done < ${HEADER}
          shopt -u nocasematch
        else
          SETTING_VAR_NAME_COMPLETE="QgsSettingsRegistry${MODULE_REGISTRY}::${SETTING_VAR_NAME}"
          SETTING_VAR_NAME_CLASS="QgsSettingsRegistry${MODULE_REGISTRY}::${SETTING_VAR_NAME}"
          SETTING_VAR_NAME_REGISTRY="${SETTING_VAR_NAME}"
          LINE_VAR_NAME="ADDSETTING_REG_H_${MODULE_REGISTRY}"
          ${GP}sed -i "${!LINE_VAR_NAME} i \ \ \ \ static const inline QgsSettingsEntry${SETTING_TYPE}${ENUM_SPEC} ${SETTING_VAR_NAME_REGISTRY} = QgsSettingsEntry${SETTING_TYPE}${ENUM_SPEC}( QStringLiteral( \"${KEY}\" ), ${PREFIX_VAR_COMPLETE}, ${DEFAULT} );" ${MODULE_REG_H}
          exit 1
        fi

        PREFIX_DECL_COUNT=$(git grep -c "static const inline char \*${PREFIX_VAR} = \"${PREFIX_STR}\";" src/core/settings/qgssettings.h | cut -d: -f2)
        if [[ -z $PREFIX_DECL_COUNT ]]; then
          ${GP}sed -i "105 i \ \ \ \ \ \ \ \ static const inline char *${PREFIX_VAR} = \"${PREFIX_STR}\";" src/core/settings/qgssettings.h
        fi

        # Add setting to registry
        HEADER_BASENAME=$(basename ${HEADER})
        LINE_VAR_NAME="ADDSETTING_REG_CPP_${MODULE_REGISTRY}"
        ${GP}sed -i "${!LINE_VAR_NAME} i \ \ addSettingsEntry( \&${SETTING_VAR_NAME_REGISTRY} );" ${MODULE_REG_CPP}
        INCLUDE_REG_COUNT=$(git grep -c "#include \"${HEADER_BASENAME}\"" ${MODULE_REG_CPP} | cut -d: -f2)
        if [[ -z $INCLUDE_REG_COUNT ]] && [[ $MODE == "class" ]]; then
          LINE_VAR_NAME="INCLUDE_REG_CPP_${MODULE_REGISTRY}"
          ${GP}sed -i "${!LINE_VAR_NAME} i #include \"${HEADER_BASENAME}\"" ${MODULE_REG_CPP}
        fi
        while read -r S_FILE; do
          if [[ -z ${S_FILE} ]]; then continue; fi
          SVARNAME=$([[ ${S_FILE} =~ ${HEADER} ]] && echo "${SETTING_VAR_NAME}" || echo "${SETTING_VAR_NAME_COMPLETE}")
          echo "fixing ${S_FILE} with $SVARNAME"
          ${GP}sed -i -r "s/(QgsSettings\(\)|QSettings\(\)|mSettings|settings|mySettings|s)(\.|->)${VALUE_GET}\( QStringLiteral. .${SETTING_ESC}. \)(, +([^ ]+))* \)${SETTING_TYPE_TO}/${SVARNAME}.${VALUE_DEFAULT_OVERRIDE_METHOD}(${VALUE_DEFAULT_OVERRIDE_VALUE})/" ${S_FILE}
          ${GP}sed -i -r "s/(QgsSettings\(\)|QSettings\(\)|mSettings|settings|mySettings|s)(\.|->)${VALUE_SET}\( QStringLiteral. .${SETTING_ESC}. \), +([^ ]+)(, +([^ ]+))? \)/${SVARNAME}.setValue( \3 )/" ${S_FILE}
          if [[ ${MODE} == registry ]]; then
            INCLUDE_IMPL_COUNT=$(git grep -c "#include \"qgssettingsregistry${MODULE_REGISTRY,,}.h\"" ${S_FILE} | cut -d: -f2)
            if [[ -z $INCLUDE_IMPL_COUNT ]]; then
              ${GP}sed -i "20 i #include \"qgssettingsregistry${MODULE_REGISTRY,,}.h\"" ${S_FILE}
            fi
          fi
        done<<<$(git grep -E -c "${SETTING_ESC}" src  | cut -d: -f1)
      fi
      git add src python tests
      git commit -m "migrate setting ${SETTING} to ${MODE} (${SETTING_TYPE}, ${MODULE_REGISTRY,,})"
    fi
  done 3< <( unbuffer ag --noaffinity --only-matching --nonumber --no-group --nocolor -G '\.cpp$' "settings\.${VALUE_GET}\( QStringLiteral\( \"[^ ]+\" \)(, +[^ ]+)+ \)${SETTING_TYPE_TO}" src/${MODULE})
done

exit 0


# cleanup
while read -r FILE; do
  if [[ $FILE =~ ^src/core/settings ]]; then
    echo "skip $FILE"
    continue
  fi
  echo "clean $FILE"
  ${GP}sed -i -r '/^ *(const *)?QgsSettings (s|mS)ettings;/d' $FILE
done<<<$(ag -c QgsSettings src/${MODULE} | cut -d: -f1)
git add src
git commit -m "clean up QgsSettings in src/${MODULE}"

exit 0

