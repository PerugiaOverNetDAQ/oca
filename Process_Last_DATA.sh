#!/bin/bash

scriptPath="/home/footpg-daq/PG_MSD/Root"
rawPath="/media/footpg-daq/datapartition/202209_PAN_data"
comprPath=$rawPath"/compressed"
calibPath=$rawPath"/calibration"


#****** PAPERO ******
LAST_CAL=`ls -lrths /media/footpg-daq/datapartition/202209_PAN_data | grep "CAL" | awk '{print $10}' | tail -1 | sed 's/.dat//'`
echo "Last calibration: "$LAST_CAL

# New runs use the DAQ label; retain BEAM in the search for older files.
LAST_BEAM=`ls -lrths /media/footpg-daq/datapartition/202209_PAN_data | grep -E "DAQ|BEAM" | awk '{print $10}' | tail -1 | sed 's/.dat//'`
echo "Last DAQ data: "$LAST_BEAM

# Strip the semantic prefix instead of using a fixed byte offset: DAQ is one
# character shorter than the historical BEAM label.
CAL_NAME_ROOTFILE=${LAST_CAL#*_CAL_}
BEAM_NUM=${LAST_BEAM#*_DAQ_}
BEAM_NUM=${BEAM_NUM#*_BEAM_}

#Delete calibration files
#rm -vf $calibPath/$CAL_NAME_ROOTFILE*
##
##Convert and compute calibration
#$scriptPath/PAPERO_convert --boards 5 $rawPath/$LAST_CAL.dat $comprPath/$LAST_CAL.root
#$scriptPath/calibration --fast --output $calibPath/$CAL_NAME_ROOTFILE $comprPath/$LAST_CAL.root
#
##Convert run file
#$scriptPath/PAPERO_convert --boards 5 $rawPath/$LAST_BEAM.dat $comprPath/$LAST_BEAM.root
#
##Clusterize
#$scriptPath/raw_clusterize --version 2020 --output analysis/PROFILE_$BEAM_NUM --calibration $calibPath/$CAL_NAME_ROOTFILE.cal  $comprPath/$LAST_BEAM.root --max_histo_ADC 1500


#*******AMS*****
LAST_AMS_DIR_CAL=`ls -lrthsd /home/footpg-daq/PG_MSD/Root/rawdata_AMS/*/ | awk '{print $10}' | tail -1`
LAST_AMS_DIR_BLOCK=`ls -lrthsd /home/footpg-daq/PG_MSD/Root/rawdata_AMS/*/ | awk '{print $10}' | tail -1`
LAST_AMS_BLOCK=`ls -lrths $LAST_AMS_DIR_BLOCK | awk '{print $10}' | tail -1`
echo $LAST_AMS_BLOCK
cal_find=0

index=1
index_max=`ls -lrths $LAST_AMS_DIR_BLOCK | wc -l`

while [ $cal_find -eq 0 ]
do
    DIM=`ls -lrts $LAST_AMS_DIR_CAL | awk '{print $6}' | tail -$index | head -1` 
    LAST_CAL_BLOCK=`ls -lrts $LAST_AMS_DIR_CAL | awk '{print $10}' | tail -$index | head -1` 
    index=$(($index+1))
    echo $DIM
    if [ $(($DIM)) -gt 10000000 ]; then
	INPUT_tmp=$LAST_AMS_DIR_CAL$LAST_CAL_BLOCK    
	cal_find=1
    fi
done

INPUT=${INPUT_tmp:41:8}
echo "LAST CAL: " $INPUT
INPUT1_tmp=$LAST_AMS_DIR_CAL$(($LAST_CAL_BLOCK+2))
INPUT1=${INPUT1_tmp:41:8}
echo "FIRST BLOCK: "$INPUT1
INPUT2_tmp=$LAST_AMS_DIR_BLOCK$LAST_AMS_BLOCK
INPUT2=${INPUT2_tmp:41:8}
echo "LAST BLOCK: "$INPUT2
#INPUT1=$1
#INPUT2=$2
#INPUT=$3

DIR=${INPUT:0:4}
FILE=${INPUT:5:8}

FILE_TO_WRITE="/home/footpg-daq/PG_MSD/Root/analysis/*_withCAL_"$DIR"."$FILE.root 
FILE_ADDED="/home/footpg-daq/PG_MSD/Root/rawdata_AMS_hacked/AMS_L0_${INPUT1:0:4}"."${INPUT1:5:8}*"

rm -fv $FILE_TO_WRITE
rm -fv $FILE_ADDED


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
