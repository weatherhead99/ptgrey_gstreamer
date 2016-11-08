#include <stdio.h>


#include <gst/gstelement.h>
#include <gst/gstbuffer.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>
#include "FlyCapture2.h"
#include <iostream>
#include <sstream>

using namespace FlyCapture2;
using namespace std;

typedef struct {
 GstPipeline *pipeline;
 GstAppSrc *src;
 GstElement *sink;
 GstElement *encoder;
 GstElement *decoder;
 GstElement *ffmpeg;
 GstElement *xvimagesink;
 GMainLoop *loop;
 guint sourceid;
 FILE *file;
}gst_app_t;

static gst_app_t gst_app;
Camera camera;

#define BUFF_SIZE (1024)

void getImagePtr(guint8 * &ptr, gint &size);

static gboolean read_data(gst_app_t *app)
{
 GstBuffer *buffer;
 guint8 *ptr;
 gint size;
 GstFlowReturn ret;

 getImagePtr(ptr, size);

 //cout << size << endl;

 // size = fread(ptr, 1, BUFF_SIZE, app->file);

 if(size == 0){
  ret = gst_app_src_end_of_stream(app->src);
  g_debug("eos returned %d at %d\n", ret, __LINE__);
  return FALSE;
}

 GstCaps *caps = NULL;
 std::stringstream video_caps_text;
 video_caps_text << "video/x-raw-rgb,bpp=(int)24,depth=(int)24,endianness=(int)4321,red_mask=(int)16711680,green_mask=(int)65280,blue_mask=(int)255,width=(int)1288,height=(int)964,framerate=(fraction)0/1";
 caps = gst_caps_from_string( video_caps_text.str().c_str() ); 

 g_object_set( G_OBJECT(app->src), "caps", caps, NULL);

 
//  TODO: replace with gst-1.0 compat
//  buffer = gst_buffer_new();
//  GST_BUFFER_MALLOCDATA(buffer) = ptr;
//  
//  gst_buffer_map();
//  GST_BUFFER_SIZE(buffer) = size;
//  GST_BUFFER_DATA(buffer) = GST_BUFFER_MALLOCDATA(buffer);
 
 buffer = gst_buffer_new_allocate(nullptr,size,nullptr);
 

 {
   GstCaps *caps_source = NULL;
   std::stringstream video_caps_text;
   video_caps_text << "video/x-raw-rgb,bpp=(int)24,depth=(int)24,endianness=(int)4321,red_mask=(int)16711680,green_mask=(int)65280,blue_mask=(int)255,width=(int)1288,height=(int)964,framerate=(fraction)0/1";
   caps_source = gst_caps_from_string( video_caps_text.str().c_str() );
   cout<<video_caps_text.str()<<endl;
   if( !GST_IS_CAPS( caps_source) ){
     cout<<"ERROR MAKING CAPS"<<endl;
     exit(1);
   }

   gst_app_src_set_caps( GST_APP_SRC( app->src ), caps_source);
//    HACK: gst-1.0 compat
//    gst_buffer_set_caps( buffer, caps_source);
   gst_caps_unref( caps_source ); 

 }

 ret = gst_app_src_push_buffer(app->src, buffer);

 if(ret !=  GST_FLOW_OK){
  g_debug("push buffer returned %d for %d bytes \n", ret, size);
  return FALSE;
 }
 else if(ret == GST_FLOW_OK){
   //cout<<"FLOW OK"<<endl;
 }

 if(!(size > BUFF_SIZE)){
   cout<<"ISSUE FOUND"<<endl;
  ret = gst_app_src_end_of_stream(app->src);
  g_debug("eos returned %d at %d\n", ret, __LINE__);
  return FALSE;
 }

 return TRUE;
}

static void start_feed (GstElement * pipeline, guint size, gst_app_t *app)
{
 if (app->sourceid == 0) {
  GST_DEBUG ("start feeding");
  app->sourceid = g_idle_add ((GSourceFunc) read_data, app);
 }
}

static void stop_feed (GstElement * pipeline, gst_app_t *app)
{
 if (app->sourceid != 0) {
  GST_DEBUG ("stop feeding");
  g_source_remove (app->sourceid);
  app->sourceid = 0;
 }
}

static void on_pad_added(GstElement *element, GstPad *pad)
{
  cout<<"PAD ADDED"<<endl;
 GstCaps *caps;
 GstStructure *str;
 gchar *name;
 GstPad *ffmpegsink;
 GstPadLinkReturn ret;

 g_debug("pad added");

//  HACK: gst-1.0 compat
//  caps = gst_pad_get_caps(pad);
 caps = gst_pad_get_current_caps(pad);
 gst_caps_make_writable(pad);
 
 
 str = gst_caps_get_structure(caps, 0);
 cout<<"CAPS: "<<str<<endl;

 g_assert(str);

 name = (gchar*)gst_structure_get_name(str);
 cout<<"NAME IS: "<<name<<endl;

 g_debug("pad name %s", name);

 if(g_strrstr(name, "video")){
//    HACK: gst-1.0 compat
//   ffmpegsink = gst_element_get_pad(gst_app.ffmpeg, "sink");
  ffmpegsink = gst_element_get_static_pad(gst_app.ffmpeg,"sink");
  g_assert(ffmpegsink);
  ret = gst_pad_link(pad, ffmpegsink);
  g_debug("pad_link returned %d\n", ret);
  gst_object_unref(ffmpegsink);
 }
 gst_caps_unref(caps);
}

static gboolean bus_callback(GstBus *bus, GstMessage *message, gpointer *ptr)
{
 gst_app_t *app = (gst_app_t*)ptr;

 switch(GST_MESSAGE_TYPE(message)){

 case GST_MESSAGE_ERROR:{
  gchar *debug;
  GError *err;

  gst_message_parse_error(message, &err, &debug);
  g_print("Error %s\n", err->message);
  g_error_free(err);
  g_free(debug);
  g_main_loop_quit(app->loop);
 }
 break;

 case GST_MESSAGE_WARNING:{
  gchar *debug;
  GError *err;
  const gchar *name;

  gst_message_parse_warning(message, &err, &debug);
  g_print("Warning %s\nDebug %s\n", err->message, debug);

  name = GST_MESSAGE_SRC_NAME(message);

  g_print("Name of src %s\n", name ? name : "nil");
  g_error_free(err);
  g_free(debug);
 }
 break;

 case GST_MESSAGE_EOS:
  g_print("End of stream\n");
  g_main_loop_quit(app->loop);
  break;

 case GST_MESSAGE_STATE_CHANGED:
  break;

 default:
  g_print("got message %s\n", \
   gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
  break;
 }

 return TRUE;
}

void PrintError( Error error )
{
    error.PrintErrorTrace();
}

void connectCamera(){
    cout<<"STARTING CONNECTION FUNCTION"<<endl;
    Error error;
    BusManager busMgr;
    unsigned int numCameras;
    PGRGuid guid;

    error = busMgr.GetNumOfCameras(&numCameras);
    if (error != PGRERROR_OK)
      {
    PrintError (error);
      }
    cout << "Number of cameras detected: " << numCameras << endl;

    for (unsigned int i=0; i < numCameras; i++)
    {
        error = busMgr.GetCameraFromIndex(i, &guid);
        if (error != PGRERROR_OK)
        {
            PrintError( error );
        }

    }

    // Connect the camera
    error = camera.Connect( &guid );
    if ( error != PGRERROR_OK )
    {
        std::cout << "Failed to connect to camera" << std::endl;     
        return;
    }
    else
      std::cout << "CONNECTED!" << std::endl;
}

void getImagePtr( guint8 * &ptr, gint &size){
  // Get the image
  Image rawImage;
  Error error = camera.RetrieveBuffer( &rawImage );
  if ( error != PGRERROR_OK )
  {
    std::cout << "capture error" << std::endl;
  }

  // convert to rgb
  Image bgrImage;
  rawImage.Convert( FlyCapture2::PIXEL_FORMAT_BGR, &bgrImage );

  // cout << rawImage.GetDataSize() << endl;

  ptr = (guint8*)g_malloc(bgrImage.GetDataSize());
  g_assert(ptr);
  memcpy( ptr,bgrImage.GetData(), bgrImage.GetDataSize() );

  size = bgrImage.GetDataSize(); 

  // ptr = bgrImage.GetData();
}

int main(int argc, char *argv[])
{
 gst_app_t *app = &gst_app;
 GstBus *bus;
 GstStateChangeReturn state_ret;

 if(argc != 2){
  printf("File name not specified\n");
  return 1;
 }

 connectCamera();
 camera.StartCapture(); 

 app->file = fopen(argv[1], "r");

 g_assert(app->file);

 gst_init(NULL, NULL);

 app->pipeline = (GstPipeline*)gst_pipeline_new("mypipeline");
 bus = gst_pipeline_get_bus(app->pipeline);
 gst_bus_add_watch(bus, (GstBusFunc)bus_callback, app);
 gst_object_unref(bus);

 app->src = (GstAppSrc*)gst_element_factory_make("appsrc", "mysrc");
 //app->encoder = gst_element_factory_make("nv_omx_h264enc", "nvidEnc");
 //app->decoder = gst_element_factory_make("decodebin", "mydecoder");
 
 //HACK: gst-1.0 compat
 app->ffmpeg = gst_element_factory_make("videoconvert", "myffmpeg");
 app->xvimagesink = gst_element_factory_make("xvimagesink", "myvsink");

 g_assert(app->src);
 //g_assert(app->encoder);
 //g_assert(app->decoder);
 g_assert(app->ffmpeg);
 g_assert(app->xvimagesink);

 g_signal_connect(app->src, "need-data", G_CALLBACK(start_feed), app);
 g_signal_connect(app->src, "enough-data", G_CALLBACK(stop_feed), app);
 //g_signal_connect(app->decoder, "pad-added", 
 // G_CALLBACK(on_pad_added), app->decoder);

 //gst_bin_add_many(GST_BIN(app->pipeline), (GstElement*)app->src, app->encoder,
 //app->decoder, app->ffmpeg, app->xvimagesink, NULL);

 gst_bin_add_many(GST_BIN(app->pipeline), (GstElement*)app->src, app->ffmpeg, app->xvimagesink, NULL);

 //if(!gst_element_link((GstElement*)app->src, app->encoder)){
 //g_warning("failed to link src anbd decoder");
 //}

 //if(!gst_element_link(app->encoder, app->decoder)){
 // g_warning("failed to link encoder and decoder");
 //}

if(!gst_element_link(app->ffmpeg, app->xvimagesink)){
 g_warning("failed to link ffmpeg and xvsink");
}

if(!gst_element_link(reinterpret_cast<GstElement*>(app->src), app->ffmpeg))
{
  g_warning("failed to link src and ffmpeg");
};

 state_ret = gst_element_set_state((GstElement*)app->pipeline, GST_STATE_PLAYING);
 g_warning("set state returned %d\n", state_ret);

 app->loop = g_main_loop_new(NULL, FALSE);

 //GstCaps *appsrcCaps = NULL;
 //appsrcCaps = gst_video_format_new_caps(GST_VIDEO_FORMAT_BGR, 1288, 964, 0, 1, 4, 3);
 //gst_app_src_set_caps(GST_APP_SRC(app->src), appsrcCaps);

 g_main_loop_run(app->loop);

 camera.StopCapture();
 camera.Disconnect();
 state_ret = gst_element_set_state((GstElement*)app->pipeline, GST_STATE_NULL);
 g_warning("set state null returned %d\n", state_ret);

 return 0;
}
