#!/bin/bash

if [ $# -eq 0 ]
  then
    echo "You have to pass: OCA CALIBRATION - OCA FILE - FIRST AMS BLOCK - LAST AMS BLOCK - AMS CALIBRATION"
    echo "****** example ******"
    echo "./Process_XXXX_DATA.sh 20221003_162322 20221003_162446 0007/719 0007/721 0007/756"
    echo "*********************"
    exit 1
fi

scriptPath="/home/footpg-daq/PG_MSD/Root"
rawPath="/media/footpg-daq/datapartition/202209_PAN_data"
comprPath=$rawPath"/compressed"
calibPath=$rawPath"/calibration"


#****** PAPERO ******
LAST_CAL="SCD_RUN00021_CAL_"$1
echo "Last calibration: "$LAST_CAL

# Files produced by the explicit `startOCA daq ...` command carry DAQ here.
LAST_BEAM="SCD_RUN00021_DAQ_"$2
echo "Last DAQ data: "$LAST_BEAM

CAL_NAME_ROOTFILE=${LAST_CAL#*_CAL_}
BEAM_NUM=${LAST_BEAM#*_DAQ_}

#Delete calibration files
rm -vf $calibPath/$CAL_NAME_ROOTFILE*
#
#Convert and compute calibration
$scriptPath/PAPERO_convert --boards 5 $rawPath/$LAST_CAL.dat $comprPath/$LAST_CAL.root
$scriptPath/calibration --fast --output $calibPath/$CAL_NAME_ROOTFILE $comprPath/$LAST_CAL.root

#Convert run file
$scriptPath/PAPERO_convert --boards 5 $rawPath/$LAST_BEAM.dat $comprPath/$LAST_BEAM.root

#Clusterize
$scriptPath/raw_clusterize --version 2020 --output analysis/PROFILE_$BEAM_NUM --calibration $calibPath/$CAL_NAME_ROOTFILE.cal  $comprPath/$LAST_BEAM.root --max_histo_ADC 1500


#*******AMS*****

INPUT1=$3
INPUT2=$4
INPUT=$5

DIR=${INPUT:0:4}
FILE=${INPUT:5:8}

#Calibration (after conversion)
./AMS_convert rawdata_AMS/$INPUT compressed/AMS_L0_$DIR"_"$FILE.root
./calibration --fast --output calibrations/L0_PS_CAL_$DIR"_"$FILE compressed/AMS_L0_$DIR"_"$FILE.root

#Join run files
./joinAMSfiles.sh $INPUT1 $INPUT2

##Convert joined files
./AMS_convert rawdata_AMS_hacked/AMS_L0_${INPUT1:0:4}"."${INPUT1:5:8}"_"${INPUT2:0:4}"."${INPUT2:5:8} compressed/AMS_L0_${INPUT1:0:4}"."${INPUT1:5:8}"_"${INPUT2:0:4}"."${INPUT2:5:8}".root"

./raw_clusterize --version 2023 --output analysis/AMS_L0_${INPUT1:0:4}"."${INPUT1:5:8}"_"${INPUT2:0:4}"."${INPUT2:5:8}"_withCAL_"$DIR"."$FILE --calibration calibrations/L0_PS_CAL_$DIR"_"$FILE.cal compressed/AMS_L0_${INPUT1:0:4}"."${INPUT1:5:8}"_"${INPUT2:0:4}"."${INPUT2:5:8}".root" --max_histo_ADC 1500


#Output
echo
echo "PAPERO data have been processed and saved in analysis/"PROFILE_$BEAM_NUM.root " file"
echo
echo "AMS data have been processed and saved in analysis/AMS_L0_"${INPUT1:0:4}"."${INPUT1:5:8}"_"${INPUT2:0:4}"."${INPUT2:5:8}"_withCAL_"$DIR"."$FILE".root"
echo
