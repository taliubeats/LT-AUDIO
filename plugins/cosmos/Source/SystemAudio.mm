#include "SystemAudio.h"

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>
#include <vector>

#if defined (__MAC_14_2)
 #import <CoreAudio/AudioHardwareTapping.h>
 #import <CoreAudio/CATapDescription.h>
 #define LT_HAS_TAPS 1
#else
 #define LT_HAS_TAPS 0
#endif

namespace cosmos
{

struct SystemAudio::Impl
{
    AudioObjectID tap = kAudioObjectUnknown, aggregate = kAudioObjectUnknown;
    AudioDeviceIOProcID proc = nullptr;
    Callback callback;
    double rate = 48000.0;
    std::vector<float> left, right;
    bool running = false;

    void destroy()
    {
        if (aggregate != kAudioObjectUnknown)
        {
            if (proc != nullptr) { AudioDeviceStop (aggregate, proc); AudioDeviceDestroyIOProcID (aggregate, proc); }
            AudioHardwareDestroyAggregateDevice (aggregate);
        }
       #if LT_HAS_TAPS
        if (@available (macOS 14.2, *))
            if (tap != kAudioObjectUnknown) AudioHardwareDestroyProcessTap (tap);
       #endif
        tap = aggregate = kAudioObjectUnknown;
        proc = nullptr;
        running = false;
    }
};

SystemAudio::SystemAudio() : impl (std::make_unique<Impl>()) {}
SystemAudio::~SystemAudio() { stop(); }

bool SystemAudio::isSupported()
{
   #if LT_HAS_TAPS
    if (@available (macOS 14.2, *)) return true;
   #endif
    return false;
}

bool SystemAudio::isRunning() const { return impl->running; }

void SystemAudio::stop() { impl->destroy(); }

static NSString* defaultOutputUID()
{
    AudioObjectID dev = kAudioObjectUnknown;
    UInt32 size = sizeof (dev);
    AudioObjectPropertyAddress a { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    if (AudioObjectGetPropertyData (kAudioObjectSystemObject, &a, 0, nullptr, &size, &dev) != noErr) return nil;
    CFStringRef uid = nullptr;
    size = sizeof (uid);
    AudioObjectPropertyAddress u { kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    if (AudioObjectGetPropertyData (dev, &u, 0, nullptr, &size, &uid) != noErr || uid == nullptr) return nil;
    return (__bridge_transfer NSString*) uid;
}

std::string SystemAudio::start (Callback cb)
{
    stop();
   #if LT_HAS_TAPS
    if (@available (macOS 14.2, *))
    {
        auto& d = *impl;
        d.callback = std::move (cb);

        // a private, un-muted tap on the whole system mix (stereo)
        CATapDescription* desc = [[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:@[]];
        desc.name = @"LT COSMOS";
        desc.privateTap = YES;
        desc.muteBehavior = CATapUnmuted;
        if (AudioHardwareCreateProcessTap (desc, &d.tap) != noErr || d.tap == kAudioObjectUnknown)
        {
            d.destroy();
            return "macOS did not allow listening to system audio. Allow it in System Settings > Privacy & Security > Screen & System Audio Recording.";
        }

        CFStringRef tapUID = nullptr;
        UInt32 size = sizeof (tapUID);
        AudioObjectPropertyAddress tu { kAudioTapPropertyUID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        AudioObjectGetPropertyData (d.tap, &tu, 0, nullptr, &size, &tapUID);

        AudioStreamBasicDescription fmt {};
        size = sizeof (fmt);
        AudioObjectPropertyAddress tf { kAudioTapPropertyFormat, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        if (AudioObjectGetPropertyData (d.tap, &tf, 0, nullptr, &size, &fmt) == noErr && fmt.mSampleRate > 0)
            d.rate = fmt.mSampleRate;

        // a private aggregate device clocked by the current output, holding only the tap
        NSMutableDictionary* agg = [@{
            @kAudioAggregateDeviceNameKey: @"LT COSMOS system audio",
            @kAudioAggregateDeviceUIDKey: [[NSUUID UUID] UUIDString],
            @kAudioAggregateDeviceIsPrivateKey: @YES,
            @kAudioAggregateDeviceIsStackedKey: @NO,
            @kAudioAggregateDeviceTapAutoStartKey: @YES,
            @kAudioAggregateDeviceTapListKey: @[ @{ @kAudioSubTapUIDKey: (__bridge NSString*) tapUID, @kAudioSubTapDriftCompensationKey: @YES } ],
        } mutableCopy];
        if (NSString* out = defaultOutputUID())
        {
            agg[@kAudioAggregateDeviceMainSubDeviceKey] = out;
            agg[@kAudioAggregateDeviceSubDeviceListKey] = @[ @{ @kAudioSubDeviceUIDKey: out } ];
        }
        if (tapUID != nullptr) CFRelease (tapUID);

        if (AudioHardwareCreateAggregateDevice ((__bridge CFDictionaryRef) agg, &d.aggregate) != noErr)
        {
            d.destroy();
            return "could not create the capture device";
        }

        Impl* self = &d;
        auto status = AudioDeviceCreateIOProcIDWithBlock (&d.proc, d.aggregate, nullptr,
            ^(const AudioTimeStamp*, const AudioBufferList* in, const AudioTimeStamp*, AudioBufferList*, const AudioTimeStamp*)
            {
                if (in == nullptr || in->mNumberBuffers == 0 || ! self->callback) return;
                // the tap arrives as one interleaved stereo buffer, or as one buffer per channel
                const auto& b0 = in->mBuffers[0];
                if (in->mNumberBuffers >= 2)
                {
                    auto n = (int) (b0.mDataByteSize / sizeof (float));
                    self->callback ((const float*) b0.mData, (const float*) in->mBuffers[1].mData, n, self->rate);
                    return;
                }
                const int ch = (int) std::max<UInt32> (1, b0.mNumberChannels);
                const int n = (int) (b0.mDataByteSize / sizeof (float)) / ch;
                if ((int) self->left.size() < n) return;   // buffers are sized before start; never allocate here
                auto* x = (const float*) b0.mData;
                for (int i = 0; i < n; ++i)
                {
                    self->left[(size_t) i] = x[i * ch];
                    self->right[(size_t) i] = x[i * ch + (ch > 1 ? 1 : 0)];
                }
                self->callback (self->left.data(), self->right.data(), n, self->rate);
            });
        d.left.assign (16384, 0.0f);
        d.right.assign (16384, 0.0f);
        if (status != noErr || AudioDeviceStart (d.aggregate, d.proc) != noErr)
        {
            d.destroy();
            return "could not start system audio capture";
        }
        d.running = true;
        return {};
    }
   #endif
    (void) cb;
    return "System audio needs macOS 14.2 or newer. On older systems, route the Mac's output through BlackHole or Loopback and pick it under INPUT.";
}

} // namespace cosmos
