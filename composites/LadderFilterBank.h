#pragma once
#include "LadderFilter.h"

#include "SqPort.h"

template <typename T>
class LadderFilterBank
{
public:
    void stepn(float sampleTime, int numChannels,
        SqInput& fc1Input, SqInput& fc2Input, SqInput& qInput, SqInput& driveInput, SqInput& edgeInput, SqInput& slopeInput,
            float fcParam, float fc1TrimParam, float fc2TrimParam,
            T volume,
            float qParam, float qTrimParam, float makeupGainParam);
           
    void step(int numChannels, bool stereoMode,
         SqInput& audioInput,  SqOutput& audioOutput);
private:
    LadderFilter<T> filters[16];

    std::shared_ptr<LookupTableParams<T>> expLookup = ObjectCache<T>::getExp2();            // Do we need more precision?

    AudioMath::ScaleFun<float> scaleFc = AudioMath::makeScalerWithBipolarAudioTrim(-5, 5);
    AudioMath::ScaleFun<float> scaleQ = AudioMath::makeScalerWithBipolarAudioTrim(0, 4);
    AudioMath::ScaleFun<float> scaleSlope = AudioMath::makeScalerWithBipolarAudioTrim(0, 3);
    AudioMath::ScaleFun<float> scaleEdge = AudioMath::makeScalerWithBipolarAudioTrim(0, 1);
};

template <typename T>
void LadderFilterBank<T>::stepn(float sampleTime, int numChannels,
    SqInput& fc1Input, SqInput& fc2Input, SqInput& qInput, SqInput& driveInput, SqInput& edgeInput, SqInput& slopeInput,
    float fcParam, float fc1TrimParam, float fc2TrimParam,
    T volume,
    float qParam, float qTrimParam, float makeupGainParam)
{
    for (int channel=0; channel < numChannels; ++channel) {
        LadderFilter<T>& filt = filters[channel];
        
        // filter Fc calc
        {
            T fcClipped = 0;
            T freqCV1 = scaleFc(
                fc1Input.getVoltage(channel),
                fcParam,
                fc1TrimParam);
            T freqCV2 = scaleFc(
                fc2Input.getVoltage(channel),
                0,
                fc2TrimParam);          // note: test second inputs
            T freqCV = freqCV1 + freqCV2 + 6;
            const T fc = LookupTable<T>::lookup(*expLookup, freqCV, true) * 10;
            const T normFc = fc * sampleTime;

            fcClipped = std::min(normFc, T(.48));
            fcClipped = std::max(fcClipped, T(.0000001));
            filt.setNormalizedFc(fcClipped);
        }

        // q and makeup gain
        {
            T res = scaleQ(
                qInput.getVoltage(0),
                qParam,
                qTrimParam);
            const T qMiddle = 2.8;
            res = (res < 2) ?
                (res * qMiddle / 2) :
                .5 * (res - 2) * (4 - qMiddle) + qMiddle;

            if (res < 0 || res > 4) fprintf(stderr, "res out of bounds %f\n", res);

            T bAmt = makeupGainParam;
            T makeupGain = 1;
            makeupGain = 1 + bAmt * (res);

            filt.setFeedback(res);
            filt.setBassMakeupGain(makeupGain);

        }
    }
    
}

template <typename T>
void LadderFilterBank<T>::step(int numChannels, bool stereoMode,
         SqInput& audioInput,  SqOutput& audioOutput)
{
    assert(!stereoMode);
    for (int channel=0; channel < numChannels; ++channel) {
        LadderFilter<T>& filt = filters[channel];

        const float input =audioInput.getVoltage(channel);
            filt.run(input);
            const float output = (float) filt.getOutput();
            audioOutput.setVoltage(output, channel);
    }
}