#include <ges/ges.h>  // at verison 1.12.4

#include <iostream>
#include <string>

#define INCLUDE_FOREGROUND_VIDEO true

int64_t __cdecl cbQueryPosition(GstElement* nleComposition, GstPipeline* gstPipeline);
GstElement* getNleCompositionElement(GESTrack* gesTrack);
bool __cdecl setInt(GESTrackElement* trackElement, const char* name, int32_t value);

int main(int nargs, char** args)
{
    // --------------------
    // Define parameters related to the composition.  We are going to show 5 slides for 2 seconds each.
    // --------------------
    int32_t numSlides = 5;
    uint64_t durationPerSlide = 2 * GST_SECOND;
    uint64_t durationTimeline = numSlides * durationPerSlide;
    int32_t canvasHeight = 1080;
    int32_t canvasWidth = 1920;
    
    //  This is where the media files are held (relative to executable).
    std::string collateralFolder = "../../collateral/";
    //std::string collateralFolder = "../collateral/";
    //std::string collateralFolder = "C:/tmp/collateral/";

    // --------------------
    // Initialize gstreamer
    // --------------------
    GError* gerror = nullptr;
    GstMessage* gstMessage = nullptr;
    GESTimeline* gesTimeline = nullptr;
    GESTrack* gesAudioTrack = nullptr;
    GESTrack* gesVideoTrack = nullptr;
    GstCaps* videoRestrictionCaps = nullptr;
    GESLayer* slideLayer = nullptr; // slideshow on background layer
    GESLayer* foregroundLayer = nullptr; // video in corner of screen
    GstPipeline* gstPipeline = nullptr;
    GstElement* nleComposition = nullptr;
    GstElement* autoVideoSink = nullptr;
    GstPad* srcPad = nullptr;
    GstPad* sinkPad = nullptr;
    GstBus* gstBus = nullptr;
    GstElement* gstQueue = nullptr;

    gst_init(&nargs, &args);
    GST_INFO("Initialized gst");

    if (!ges_init())
    {
        GST_ERROR("Failed to initialize ges");
        return 1;
    }
    GST_INFO("Initialized ges");

    // -------------------
    // Build GESTimeline
    // -------------------
    gesTimeline = ges_timeline_new();

    /*
    //  Create the audio track.
    gesAudioTrack = GES_TRACK(ges_audio_track_new());
    if (!ges_timeline_add_track(gesTimeline, gesAudioTrack))
    {
        GST_ERROR("Failed to add audio track");
        return 1;
    }
    */

    //  Create the video track.
    gesVideoTrack = GES_TRACK(ges_video_track_new());
    if (!ges_timeline_add_track(gesTimeline, gesVideoTrack))
    {
        GST_ERROR("Failed to add video track");
        return 1;
    }
    videoRestrictionCaps = gst_caps_new_simple("video/x-raw",
        "height", G_TYPE_INT, canvasHeight,
        "width", G_TYPE_INT, canvasWidth,
        "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
        nullptr);
    ges_track_set_restriction_caps(gesVideoTrack, videoRestrictionCaps);

    //  In the "slideLayer" we are showing 5 slides at 2 seconds each.
    slideLayer = ges_layer_new();
    if (!ges_timeline_add_layer(gesTimeline, slideLayer))
    {
        GST_ERROR("Failed to add slide layer to timeline.");
        return 1;
    }
    ges_layer_set_priority(slideLayer, 10ul);
    for (int iSlide = 0; iSlide < numSlides; iSlide++)
    {
        std::string slideFile = collateralFolder + "slide" + std::to_string(iSlide + 1) + ".jpg";
        char* uri = gst_filename_to_uri(slideFile.c_str(), &gerror);
        if (gerror)
        {
            GST_ERROR("Failed to get uri for file: %s", slideFile.c_str());
            return 1;
        }
        GESUriClip* clip = ges_uri_clip_new(uri);
        if (!ges_layer_add_clip(slideLayer, (GESClip*)clip))
        {
            GST_ERROR("Failed to add clip to slide layer, uri=%s", uri);
            return 1;
        }
        
        //  time projection
        ges_timeline_element_set_start((GESTimelineElement*)(clip), iSlide*durationPerSlide);
        ges_timeline_element_set_duration((GESTimelineElement*)(clip), durationPerSlide);

        //  get the GESVideoSource for the clip
        GList* videoSources = ges_clip_find_track_elements(
            GES_CLIP(clip),
            gesVideoTrack,
            GESTrackType::GES_TRACK_TYPE_VIDEO,
            G_TYPE_NONE);

        if (nullptr == videoSources || nullptr == videoSources->data)
        {
            GST_ERROR("Failed to get video source for slide %ld", iSlide);
            return 1;
        }

        GESVideoSource* videoSource = (GESVideoSource*)(videoSources->data);
        g_list_free(videoSources);
        videoSources = nullptr;

        //  rect projection
        if (!setInt((GESTrackElement*)videoSource, "posx", 0)) return 1;
        if (!setInt((GESTrackElement*)videoSource, "posy", 0)) return 1;
        if (!setInt((GESTrackElement*)videoSource, "width", canvasWidth)) return 1;
        if (!setInt((GESTrackElement*)videoSource, "height", canvasHeight)) return 1;
    }

    //  In the "foregroundLayer" we are showing a 10 second video in the corner of the screen.
    if (INCLUDE_FOREGROUND_VIDEO)
    {
        foregroundLayer = ges_layer_new();
        if (!ges_timeline_add_layer(gesTimeline, foregroundLayer))
        {
            GST_ERROR("Failed to add foreground layer to timeline.");
            return 1;
        }
        ges_layer_set_priority(foregroundLayer, 1ul);
        std::string videoFile = collateralFolder + "v0.m4v";
        char* uri = gst_filename_to_uri(videoFile.c_str(), &gerror);
        if (gerror)
        {
            GST_ERROR("Failed to get uri for file: %s", videoFile.c_str());
            return 1;
        }
        GESUriClip* clip = ges_uri_clip_new(uri);
        if (!ges_layer_add_clip(foregroundLayer, (GESClip*)clip))
        {
            GST_ERROR("Failed to add clip to foreground layer, uri=%s", uri);
            return 1;
        }

        //  time projection
        ges_timeline_element_set_start((GESTimelineElement*)(clip), 0);
        ges_timeline_element_set_duration((GESTimelineElement*)(clip), 10ull*GST_SECOND);
        ges_timeline_element_set_inpoint((GESTimelineElement*)(clip), 0);

        //  get the GESVideoSource for the clip
        GList* videoSources = ges_clip_find_track_elements(
            GES_CLIP(clip),
            gesVideoTrack,
            GESTrackType::GES_TRACK_TYPE_VIDEO,
            G_TYPE_NONE);

        if (nullptr == videoSources || nullptr == videoSources->data)
        {
            GST_ERROR("Failed to get video source for clip $s", videoFile.c_str());
            return 1;
        }

        GESVideoSource* videoSource = (GESVideoSource*)(videoSources->data);
        g_list_free(videoSources);
        videoSources = nullptr;

        //  rect projection (quarter width, quarter height), lower-right corner
        if (!setInt((GESTrackElement*)videoSource, "posx", 1443)) return 1;
        if (!setInt((GESTrackElement*)videoSource, "posy", 810)) return 1;
        if (!setInt((GESTrackElement*)videoSource, "width", 477)) return 1;
        if (!setInt((GESTrackElement*)videoSource, "height", 270)) return 1;
    }

    // -------------------
    // Build GstPipeline
    //
    // NOTE:  I need to use a GstPipeline (not a GESPipeline) because in my actual project, I am outputting a bunch
    // of distinct encodings from a single pipeline.
    // -------------------
    gstPipeline = (GstPipeline*)gst_pipeline_new("whatever");

    // Make the timeline and attach it to the pipeline.
    if (!gst_bin_add((GstBin*)gstPipeline, (GstElement*)gesTimeline))
    {
        GST_ERROR("Failed to add timeline to pipeline.");
        return 1;
    }

    //  It is necessary to connect the "query-position" signal to each track because of a known bug:
    //  https://bugzilla.gnome.org/show_bug.cgi?id=796754
    nleComposition = getNleCompositionElement(gesVideoTrack);
    g_signal_connect(nleComposition, "query-position", G_CALLBACK(cbQueryPosition), gstPipeline);

    //  Put a queue after the timeline.
    gstQueue = gst_element_factory_make("queue", "queue0");
    if (nullptr == gstQueue)
    {
        GST_ERROR("Failed to create queue.");
        return 1;
    }
    gst_bin_add((GstBin*)gstPipeline, gstQueue);

    //  Link video track to queue
    srcPad = ges_timeline_get_pad_for_track(gesTimeline, gesVideoTrack);
    sinkPad = gst_element_get_compatible_pad(gstQueue, srcPad, nullptr);
    if (!GST_PAD_LINK_SUCCESSFUL(gst_pad_link(srcPad, sinkPad)))
    {
        GST_ERROR("Failed to link timeline video track to queue.");
        return 1;
    }

    //  Attach a video sink so we can see the output.
    autoVideoSink = gst_element_factory_make("autovideosink", "sink");
    if (nullptr == autoVideoSink)
    {
        GST_ERROR("Failed to create autovideosink.");
        return 1;
    }
    gst_bin_add((GstBin*)gstPipeline, autoVideoSink);

    //  Link queue to autovideosink
    if (!gst_element_link(gstQueue, autoVideoSink))
    {
        GST_ERROR("Failed to link queue to autovideosink.");
        return 1;
    }

    // -------------------
    // Run the pipeline
    // -------------------
    bool loopShouldTerminate = false;    

    //  Change the state asynchronously.
    gst_element_set_state((GstElement*)gstPipeline, GST_STATE_PLAYING);

    //  Wait to confirm the desired state change.
    GstState oldState, pendingState, currentState;
    GstStateChangeReturn ret = gst_element_get_state((GstElement*)gstPipeline,
        &currentState, &pendingState, 10ull*GST_SECOND);
    if (currentState != GST_STATE_PLAYING)
    {
        GST_ERROR("Pipeline state should be PLAYING.  It is %s.", gst_element_state_get_name(currentState));
        gst_element_set_state((GstElement*)gstPipeline, GST_STATE_NULL);
        gst_element_get_state((GstElement*)gstPipeline, &currentState, &pendingState, 10 * GST_SECOND);
        
        GST_INFO("Pipeline stopped");
        return 1;
    }

    int64_t observedDuration = GST_CLOCK_TIME_NONE;
    int64_t observedPosition = GST_CLOCK_TIME_NONE;
    gchar* debugInfo = nullptr;
    int output = 0;

    //  Main loop
    gstBus = gst_element_get_bus((GstElement*)gstPipeline);
    while (!loopShouldTerminate)
    {
        gstMessage = gst_bus_timed_pop_filtered(gstBus, GST_SECOND,
            (GstMessageType)(   GST_MESSAGE_DURATION
                              | GST_MESSAGE_EOS
                              | GST_MESSAGE_ERROR
                              | GST_MESSAGE_STATE_CHANGED));

        if (gstMessage)
        {
            switch (GST_MESSAGE_TYPE(gstMessage))
            {
            case GST_MESSAGE_DURATION:
            {
                if (gst_element_query_duration((GstElement*)gstPipeline, GST_FORMAT_TIME, &observedDuration))
                {
                    GST_INFO("Pipeline duration is %" GST_TIME_FORMAT "", GST_TIME_ARGS(observedDuration));
                }
                else
                {
                    GST_WARNING("Duration query failed.");
                }
            } break;
            case GST_MESSAGE_ERROR:
            {
                gst_message_parse_error(gstMessage, &gerror, &debugInfo);
                if (debugInfo)
                {
                    GST_ERROR("Error received from element %s: %s.\n%s",
                        GST_OBJECT_NAME(gstMessage->src), gerror->message, debugInfo);
                }
                else
                {
                    GST_ERROR("Error received from element %s: %s.",
                        GST_OBJECT_NAME(gstMessage->src), gerror->message);
                }
                g_clear_error(&gerror);
                g_free(debugInfo);
                debugInfo = nullptr;
                loopShouldTerminate = true;
                output = 1;
            } break;
            case GST_MESSAGE_EOS:
            {
                GST_INFO("Loop ended normally.  EOS reached.");
                loopShouldTerminate = true;
            } break;
            case GST_MESSAGE_STATE_CHANGED:
            {
                if (GST_MESSAGE_SRC(gstMessage) == GST_OBJECT(gstPipeline))
                {
                    gst_message_parse_state_changed(gstMessage, &oldState, &currentState, &pendingState);
                    GST_INFO("Pipeline state changed from %s to %s",
                        gst_element_state_get_name(oldState), gst_element_state_get_name(currentState));
                }
            } break;
            default:
            {
                GST_ERROR("Unhandled message type %s", gst_message_type_get_name(GST_MESSAGE_TYPE(gstMessage)));
            }
            }
            gst_mini_object_unref((GstMiniObject*)gstMessage);
            gstMessage = nullptr;
        }
        else if (!loopShouldTerminate && GST_STATE_PLAYING == currentState)
        {
            // Query the stream position (in nanoseconds).
            if (!gst_element_query_position(
                (GstElement*)gstPipeline,
                GST_FORMAT_TIME,
                &observedPosition))
            {
                GST_WARNING("Query failed for pipeline position.");
            }

            // Query the stream duration (in nanoseconds) if we don't have it already.
            if (!GST_CLOCK_TIME_IS_VALID(observedDuration))
            {
                if (!gst_element_query_duration(
                    (GstElement*)gstPipeline,
                    GST_FORMAT_TIME,
                    &observedDuration))
                {
                    GST_WARNING("Query failed for pipeline duration.");
                }
            }
        }

        GST_INFO("Position %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT "",
            GST_TIME_ARGS(observedPosition), GST_TIME_ARGS(observedDuration));
    }

    GST_INFO("Program ended successfully");
    return 0;
}

#pragma warning( push )
#pragma warning( disable:4100 ) // formal parameters are not referenced in the method below.
int64_t __cdecl cbQueryPosition(GstElement* nleComposition, GstPipeline* gstPipeline)
{
    int64_t position;

    if (gst_element_query_position(reinterpret_cast<GstElement*>(gstPipeline), GST_FORMAT_TIME, &position))
    {
        return position;
    }

    return GST_CLOCK_TIME_NONE;
}
#pragma warning( pop )

GstElement* getNleCompositionElement(GESTrack* gesTrack)
{
    GstElement* output = nullptr;

    if (gesTrack != nullptr)
    {
        GstElement* gstElement;
        GstElementFactory* gstElementFactory;
        const char* elementFactoryName;

        GValue item = G_VALUE_INIT;
        GstIterator* iterator = gst_bin_iterate_elements((GstBin*)gesTrack);
        bool done = false;
        while (!done)
        {
            switch (gst_iterator_next(iterator, &item))
            {
            case GST_ITERATOR_OK:
                gstElement = reinterpret_cast<GstElement*>(g_value_get_object(&item));
                gstElementFactory = gst_element_get_factory(gstElement);
                if (gstElementFactory)
                {
                    elementFactoryName = gst_object_get_name(reinterpret_cast<GstObject*>(gstElementFactory));
                    //if (boost::iequals(factoryName, elementFactoryName))
                    if(0==std::strcmp("nlecomposition", elementFactoryName))
                    {
                        output = gstElement;
                        done = true;
                    }
                }
                g_value_reset(&item);
                break;
            case GST_ITERATOR_RESYNC:
                gst_iterator_resync(iterator);
                break;
            default:
                done = true;
                break;
            }
        }
    }

    return output;
}

bool __cdecl setInt(GESTrackElement* trackElement, const char* name, int32_t value)
{
    bool output = true;
    
    GValue val = G_VALUE_INIT;

    g_value_init(&val, G_TYPE_INT);
    g_value_set_int(&val, value);
    if (!ges_track_element_set_child_property(trackElement, name, &val))
    {
        GST_ERROR("Failed to set %s to value of %ld", name, value);
        output = false;
    }
    return output;
}