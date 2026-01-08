/*

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/tpl.h>

#define DEFAULT_FIFO_SIZE	(256*1024)

#include "sprites/cursor0.png"


static GXRModeObj *rmode = NULL;
static void *frameBuffer[2] = { NULL, NULL};

//cursor struct
typedef struct {
	int x,y;			// screen coordinates
	int image;
}Cursor;

Cursor cursors[4];

void drawSpriteTex( int x, int y, int width, int height, int image );

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	
	////general vid setup
	
	// Initialise the video system
	VIDEO_Init();
	
	// This function initialises the attached controllers
	WPAD_Init();

	//tell wpad to output the IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);

	//// Sprite Drawing Setup
	u32	fb; 	// initial framebuffer index
	u32 first_frame;
	f32 yscale;
	u32 xfbHeight;
	Mtx44 perspective;
	Mtx GXmodelView2D;
	void *gp_fifo = NULL;

	GXColor background = {0, 0, 0, 0xff};

	int i;

	VIDEO_Init();

	rmode = VIDEO_GetPreferredMode(NULL);

	fb = 0;
	first_frame = 1;
	// allocate 2 framebuffers for double buffering
	frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(frameBuffer[fb]);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	fb ^= 1;

	// setup the fifo and then init the flipper
	gp_fifo = memalign(32,DEFAULT_FIFO_SIZE);
	memset(gp_fifo,0,DEFAULT_FIFO_SIZE);

	GX_Init(gp_fifo,DEFAULT_FIFO_SIZE);

	// clears the bg to color and clears the z buffer
	GX_SetCopyClear(background, 0x00ffffff);

	// other gx setup
	GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
	yscale = GX_GetYScaleFactor(rmode->efbHeight,rmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,GX_TRUE,rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering,((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	if (rmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);


	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(frameBuffer[fb],GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);


	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);


	GX_InvalidateTexAll();

	TPLFile spriteTPL;
	TPL_OpenTPLFromMemory(&spriteTPL, (void *)cursor0_tpl,cursor0_tpl_size);
	TPL_GetTexture(&spriteTPL,cursor0,&texObj);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);

	guOrtho(perspective,0,479,0,639,0,300);
	GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

	WPAD_Init();

	srand(time(NULL));


	//main loop
	while(1) 
	{
		//gets controller states/inputs
		WPAD_ScanPads();
		WPADData* data = WPAD_Data(i);
		
		// exit program
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) exit(0);
		
		////wii ir control
		//reset console location to 5th row
		printf("\x1b[5;0H");

		// Call WPAD_ScanPads each loop, this reads the latest controller states
		WPAD_ScanPads();
		
		for (int i = 0; i <= 3; i++)
		{
			//clear line and print the wiimote's data
			printf("\33[2K\r");
			WPADData* data = WPAD_Data(i);
			if(data->data_present)
				printf("wiimote! %d: x -> %f y-> %f\n", i, data->ir.x, data->ir.y);
			else
				printf("wiimote! %d: Disconnected\n", i);
		}

		// WPAD_ButtonsDown tells us which buttons were pressed in this loop
		// this is a "one shot" state which will not fire again until the button has been released
		u32 pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		
		//clear screen and whatnot
		GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
		GX_InvVtxCache();
		GX_InvalidateTexAll();

		GX_ClearVtxDesc();
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

		guMtxIdentity(GXmodelView2D);
		guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -5.0F);
		GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX0);
		
		drawSpriteTex( (int)data->ir.x, (int)data->ir.y, 32, 32, 0 );
		GX_DrawDone();
		
		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
		GX_SetAlphaUpdate(GX_TRUE);
		GX_SetColorUpdate(GX_TRUE);
		GX_CopyDisp(frameBuffer[fb],GX_TRUE);
		
		

		VIDEO_SetNextFramebuffer(frameBuffer[fb]);
		if(first_frame) {
			VIDEO_SetBlack(false);
			first_frame = 0;
		}
		VIDEO_Flush();
		VIDEO_WaitVSync();
		// wait for the next frame
		VIDEO_WaitVSync();
		fb ^= 1;		// flip framebuffer, so swapping the memory so that what's on screen is now free to draw on and what's drawn is now on screen
	}

	return 0;
}

//---------------------------------------------------------------------------------
void drawSpriteTex( int x, int y, int width, int height, int image ) {
//---------------------------------------------------------------------------------

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);			// Draw A Quad
		GX_Position2f32(x, y);					// Top Left
		GX_TexCoord2f32(0,0);
		GX_Position2f32(x+width-1, y);			// Top Right
		GX_TexCoord2f32(1,0);
		GX_Position2f32(x+width-1,y+height-1);	// Bottom Right
		GX_TexCoord2f32(1,1);
		GX_Position2f32(x,y+height-1);			// Bottom Left
		GX_TexCoord2f32(0,1);
	GX_End();									// Done Drawing The Quad

}

*/