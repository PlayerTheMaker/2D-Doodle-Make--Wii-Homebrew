/*---------------------------------------------------------------------------------

	Simple demonstration of sprites using textured quads

---------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/tpl.h>

#include "textures_tpl.h"
#include "textures.h"

#define DEFAULT_FIFO_SIZE	(256*1024)

static void *frameBuffer[2] = { NULL, NULL};
static GXRModeObj *rmode;

#define NUM_SPRITES 100

//cursor struct
typedef struct {
	int x,y;			// screen coordinates
	bool connected;
	bool free;
	u32 pressedInputs, heldInputs, releasedInputs;
	int lastX,lastY;
	int item;
}Cursor;

typedef struct {
	int x,y,x2,y2;
}Line;

GXTexObj texObj;

#define TEXTURE_SIZE 64
void drawSquareSprite(float tx, float ty, float tscale, float x, float y, float scale, float rot);
void drawLine(float x, float y, float x2, float y2, float width, bool grey);

//---------------------------------------------------------------------------------
int main( int argc, char **argv ){
//---------------------------------------------------------------------------------
	u32	fb; 	// initial framebuffer index
	u32 first_frame;
	f32 yscale;
	u32 xfbHeight;
	Mtx44 perspective;
	Mtx GXmodelView2D;
	void *gp_fifo = NULL;

	GXColor background = {0, 0, 0, 0xff};

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
	TPL_OpenTPLFromMemory(&spriteTPL, (void *)textures_tpl,textures_tpl_size);
	TPL_GetTexture(&spriteTPL,ballsprites,&texObj);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);

	guOrtho(perspective,0,479,0,639,0,300);
	GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

	// Initialise the console, required for printf
	CON_Init(frameBuffer[0], 20, 20, rmode->fbWidth-20, rmode->xfbHeight-20, rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	
	//controller
	WPAD_Init();
	//tell wpad to output the IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);

	//random num generator init
	srand(time(NULL));
	
	//add cursors
	Cursor cursors[4];
	Cursor defaultCursor = {.x=1000,.y=100,.free=true,.connected=false,.item=0,.lastX=0,.lastY=0};
	for (int i=0; i<=3; i++){
		
		cursors[i] = defaultCursor;
	}
	
	//object pools
	int bgLineCount = 0;
	Line bgLines[2500];
	int lineCount = 0;
	Line lines[2500];

	float placeholder = 0;

	while(1) {
		
		//GAME CALCULATIONS
		WPAD_ScanPads();

		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) exit(0);
		
		
		
		//get input data
		for(int i=0; i<=3; i++){
			WPADData* data = WPAD_Data(i);
			if(data->data_present){
				cursors[i].x = data->ir.x;
				cursors[i].y = data->ir.y;
				cursors[i].connected = true;
				cursors[i].pressedInputs = WPAD_ButtonsDown(0);
				cursors[i].heldInputs = WPAD_ButtonsHeld(0);
				cursors[i].releasedInputs = WPAD_ButtonsUp(0);
			}else{
				cursors[i] = defaultCursor;
			}
		}
		
		
		//cursor add elements
		for(int i=0; i<=3; i++){
			if(cursors[i].connected){
				Cursor* currCursor = &cursors[i];
				
				if(currCursor->pressedInputs & WPAD_BUTTON_A){
					if(currCursor->item <= 1){
						cursors[i].lastX = currCursor->x;
						cursors[i].lastY = currCursor->y;
					}else{
						
					}
				}
				
				if(currCursor->item <= 1 && currCursor->heldInputs & WPAD_BUTTON_A){
					if(sqrtf(pow(currCursor->x - currCursor->lastX, 2)+pow(currCursor->y - currCursor->lastY, 2)) > 10){
						if(currCursor->item == 0 && lineCount < 2500){
							lines[lineCount] = (Line){.x=currCursor->x,.y=currCursor->y,.x2=currCursor->lastX,.y2=currCursor->lastY};
							lineCount++;
						}
						if(currCursor->item == 1 && bgLineCount < 2500){
							bgLines[bgLineCount] = (Line){.x=currCursor->x,.y=currCursor->y,.x2=currCursor->lastX,.y2=currCursor->lastY};
							bgLineCount++;
						}
						cursors[i].lastX = currCursor->x;
						cursors[i].lastY = currCursor->y;
					}
				}
				
			}
		}
		
		
		//DRAW!
		
		//def not where I'm supposed to put print statements lol
		/*
		printf("\33[2K\r");
		if(data->data_present){
			printf("wiimote %d: x -> %f y-> %f angle -> %f\n", 0, data->ir.x, data->ir.y, data->ir.angle);
		}
		*/
		//printf("\33[2K\r");
		//printf("%d, %f",lineCount,lines[0].x2);
		
		GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
		GX_InvVtxCache();
		GX_InvalidateTexAll();

		GX_ClearVtxDesc();
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

		guMtxIdentity(GXmodelView2D);
		guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -5.0F);
		GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX0);
		
		//background giant sprite
		drawSquareSprite(0,1,1, 320,240,100,0);
		
		//time to actually draw the game!
		
		//bg lines
		for(int i = 0; i < bgLineCount; i++){
			Line* line = &bgLines[i];
			drawLine(line->x,line->y,line->x2,line->y2,3,true);
		}
		//lines
		for(int i = 0; i < lineCount; i++){
			Line* line = &lines[i];
			drawLine(line->x,line->y,line->x2,line->y2,3,false);
		}
		
		//draw cursors and half-drawns
		for(int i=0; i<=3; i++){
			Cursor* currCursor = &cursors[i];
			if (currCursor->connected){
				if((currCursor->item <= 1) && (currCursor->heldInputs & WPAD_BUTTON_A)){
					drawLine(currCursor->x,currCursor->y,currCursor->lastX,currCursor->lastY,3, currCursor->item == 1);
				}
				drawSquareSprite(i+0.025f,0.025f,0.95f, currCursor->x,currCursor->y,1.5f,0);
			}
		}
		
		//to make line count progress bar
		drawLine(0, 0, ((float)lineCount/2500)*640, 0, 10, true);
			
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
		fb ^= 1;		// flip framebuffer
	}
	return 0;
}

//---------------------------------------------------------------------------------
void drawSquareSprite(float tx, float ty, float tscale, float x, float y, float scale, float rot) {
//---------------------------------------------------------------------------------
	
	float tleft = (tx*16)/TEXTURE_SIZE;
	float ttop = (ty*16)/TEXTURE_SIZE;
	float tlength = (tscale*16)/TEXTURE_SIZE;
	float drawscale = tscale*16*scale*0.5;
	
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);			// Draw A Quad
		GX_Position2f32(x-cos(rot+M_PI/4)*drawscale, y-sin(rot+M_PI/4)*drawscale);			// Top Left
		GX_TexCoord2f32(tleft,ttop);
		GX_Position2f32(x-cos(rot+3*(M_PI/4))*drawscale, y-sin(rot+3*(M_PI/4))*drawscale);	// Top Right
		GX_TexCoord2f32(tleft+tlength,ttop);
		GX_Position2f32(x-cos(rot+5*(M_PI/4))*drawscale, y-sin(rot+5*(M_PI/4))*drawscale);	// Bottom Right
		GX_TexCoord2f32(tleft+tlength,ttop+tlength);
		GX_Position2f32(x-cos(rot-M_PI/4)*drawscale, y-sin(rot-M_PI/4)*drawscale);			// Bottom Left
		GX_TexCoord2f32(tleft,ttop+tlength);
	GX_End();									// Done Drawing The Quad

}


void drawLine(float x, float y, float x2, float y2, float width, bool grey){
	
	float perpendicular = atan2f(y2-y,x2-x)-0.5f*(float)M_PI;
	
	float xOff = cosf(perpendicular)*width*.5f;
	float yOff = sinf(perpendicular)*width*.5f;
	
	float greyOff = 0;
	if(grey) greyOff = 1;
	
	//printf("\33[2K\r");
	//printf("%f",xOff);
	
	float tleft = (1.1f*16+greyOff*16)/TEXTURE_SIZE;
	float ttop = (1.1f*16)/TEXTURE_SIZE;
	float tlength = (.1f*16)/TEXTURE_SIZE;
	
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);			// Draw A Quad
		GX_Position2f32(x+xOff, y+yOff);					// Top Left
		GX_TexCoord2f32(tleft,ttop);
		GX_Position2f32(x2+xOff, y2+yOff);			// Top Right
		GX_TexCoord2f32(tleft+tlength,ttop);
		GX_Position2f32(x2-xOff,y2-yOff);	// Bottom Right
		GX_TexCoord2f32(tleft+tlength,ttop+tlength);
		GX_Position2f32(x-xOff,y-yOff);			// Bottom Left
		GX_TexCoord2f32(tleft,ttop+tlength);
	GX_End();									// Done Drawing The Quad
}