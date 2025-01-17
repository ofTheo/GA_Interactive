#include "grafPlayerApp.h"

//--------------------------------------------------------------
GrafPlayerApp::GrafPlayerApp(){


}

GrafPlayerApp::~GrafPlayerApp(){

	ofRemoveListener(ofEvents.mouseMoved, this, &GrafPlayerApp::mouseMoved);
	ofRemoveListener(ofEvents.mousePressed, this, &GrafPlayerApp::mousePressed);
	ofRemoveListener(ofEvents.mouseReleased, this, &GrafPlayerApp::mouseReleased);
	ofRemoveListener(ofEvents.mouseDragged, this, &GrafPlayerApp::mouseDragged);
	ofRemoveListener(ofEvents.keyPressed, this, &GrafPlayerApp::keyPressed);
	ofRemoveListener(ofEvents.keyReleased, this, &GrafPlayerApp::keyReleased);

}

//--------------------------------------------------------------
void GrafPlayerApp::setup(){

	
	ofxXmlSettings xmlUser;
	xmlUser.loadFile("projects/user.xml");
	string username = xmlUser.getValue("params:user","default");
	
	pathToSettings = "projects/"+username+"/settings/";//"settings/default/";
	myTagDirectory = "projects/"+username+"/tags/";
	
	ofAddListener(ofEvents.mouseMoved, this, &GrafPlayerApp::mouseMoved);
	ofAddListener(ofEvents.mousePressed, this, &GrafPlayerApp::mousePressed);
	ofAddListener(ofEvents.mouseReleased, this, &GrafPlayerApp::mouseReleased);
	ofAddListener(ofEvents.mouseDragged, this, &GrafPlayerApp::mouseDragged);
	ofAddListener(ofEvents.keyPressed, this, &GrafPlayerApp::keyPressed);
	ofAddListener(ofEvents.keyReleased, this, &GrafPlayerApp::keyReleased);

	screenW			= 1024;
	screenH			= 768;
	loadScreenSettings();
	
	mode			= PLAY_MODE_LOAD;
	lastX			= 0;
	lastY			= 0;
	bShowPanel		= true;
	bRotating		= false;
	bShowName		= false;
	bShowTime		= false;
	bUseFog			= true;
	bUseMask		= true;
	bTakeScreenShot = false;
	bUseGravity		= true;
	bUseAudio		= true;
	bUseArchitecture= true;
	
	lastTimePointAddedF = ofGetElapsedTimef();
	
	prevStroke		= 0;
	currentTagID	= 0;
	waitTime		= 2.f;
	waitTimer		= waitTime;
	rotationY		= -45;
	tagMoveForce	= .1;
	tagPosVel.set(0,0,0);
	
	smoothX			= 0.0;
	smoothY			= 0.0;
	//myTagDirectory = TAG_DIRECTORY;
	
	fontSS.loadFont("fonts/frabk.ttf",9);
	fontS.loadFont("fonts/frabk.ttf",14);
	fontL.loadFont("fonts/frabk.ttf",22);
	imageMask.loadImage("images/mask.jpg");
	
	float FogCol[3]={0,0,0};
    glFogfv(GL_FOG_COLOR,FogCol);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_DENSITY, 0.05f);
	fogStart = 370;
    fogEnd   = 970;
	
	//ofxXmlSettings xml;
	//xml.loadFile(pathToSettings+"directorySettings");
	//myTagDirectory = xml.getValue("directory", TAG_DIRECTORY);
	
	// controls
	setupControlPanel();
	updateControlPanel();
	
	// load tgs
	preLoadTags();
	particleDrawer.setup(screenW,screenH);
	
	// fbo
	fbo.allocate(screenW,screenH );
	pWarper.initWarp( screenW,screenH,screenW*WARP_DIV,screenH*WARP_DIV );
	pWarper.recalculateWarp();
	
	
	pWarper.loadFromXml(pathToSettings+"warper.xml");
	
	// audio
	if(bUseAudio) audio.setup();
	
	// interactive architecture setup
	if(bUseArchitecture)
	{
		archPhysics.setup(screenW,screenH);
		archPhysics.archImage.loadImage(pathToSettings+"arch.jpg");
		archPhysics.loadFromXML(pathToSettings+"architecture.xml");
		createWarpedArchitecture();
	}
	
	loadTags();

	// temp
	//panel.setSelectedPanel("FBO Warper");
	
}

static float lastTime = 0.f;

//--------------------------------------------------------------
void GrafPlayerApp::update(){

	dt  = ofGetElapsedTimef()-lastTime;
	lastTime  = ofGetElapsedTimef();
		
	if( panel.getValueB("useLaser") ){
		if( panel.getValueB("bUseClearZone") ){
			laserTracker.setUseClearZone(true);			
			checkLaserHitState();		
		}else{
			laserTracker.setUseClearZone(false);
		}
		
		if( panel.getValueB("laserMode") ){
			mode = PLAY_MODE_RECORD;
		}else{
			mode = PLAY_MODE_PLAY;
		}
	}else{
			mode = PLAY_MODE_PLAY;
	}
	
	
	bool bTrans = false;
	if( currentTagID >= tags.size() ){
		currentTagID = tags.size()-1;
		if( currentTagID < 0 ){
			currentTagID = 0;
			loadTags();
		}
	}
		

	if( mode == PLAY_MODE_PLAY && tags.size() > 0 )
	{
		
		//---- set drawing data for render
		if( drawer.bSetupDrawer )
			drawer.setup( &tags[currentTagID], tags[currentTagID].distMax );

		//---- update tag playing state
		if( !myTagPlayer.bDonePlaying )					
		{
			myTagPlayer.update(&tags[currentTagID]);	// normal play, update tag
		
		}else if( !myTagPlayer.bPaused && myTagPlayer.bDonePlaying && waitTimer > 0)			   
		{
			waitTimer -= dt;	// pause time after drawn, before fades out
		}
		else if ( !myTagPlayer.bPaused && myTagPlayer.bDonePlaying && (drawer.alpha > 0 || particleDrawer.alpha > 0))			  		
		{
			updateTransition(0);
		}
		else if (  !myTagPlayer.bPaused && myTagPlayer.bDonePlaying )							
		{
			resetPlayer(1);	// setup for next tag
		}
	
		
		//---------- AUDIO applied
		if( bUseAudio) updateAudio();
		
		
		//--------- ARCHITECTURE
		if( bUseArchitecture ) updateArchitecture();
		
		
		//--------- PARTICLES
		updateParticles();
			
		
		//THEO
		
		if( panel.getValueB("useLaser") ){		
			handleLaserPlayback();
		}else{
			//--------- TAG ROTATION + POSITION
			if(bRotating && !myTagPlayer.bPaused ) rotationY += panel.getValueF("ROT_SPEED")*dt;
			
			// update pos / vel
			tags[currentTagID].position.x += tagPosVel.x;
			tags[currentTagID].position.y += tagPosVel.y;
			
			tagPosVel.x -= .1*tagPosVel.x;
			tagPosVel.y -= .1*tagPosVel.y;
		}
	}
	else if( mode == PLAY_MODE_RECORD ){
		handleLaserRecord();
	}
		
	// controls
	if( bShowPanel ) updateControlPanel();
	
	panel.clearAllChanged();
}

void GrafPlayerApp::updateParticles(){
	
	int lastStroke = myTagPlayer.getCurrentStroke();
	int lastPoint  = myTagPlayer.getCurrentId();
	
	if( prevStroke != lastStroke )	myTagPlayer.bReset = true; 
	if( lastPoint <= 0 )			myTagPlayer.bReset = true;
	if( tags[currentTagID].myStrokes[ lastStroke].pts.size()-1 == lastPoint ) myTagPlayer.bReset = true;
	
	particleDrawer.update( myTagPlayer.getCurrentPoint(),myTagPlayer.getVelocityForTime(&tags[currentTagID]),  dt,  myTagPlayer.bReset);
	
	myTagPlayer.bReset = false; // important so no particle error on first stroke
	prevStroke		= myTagPlayer.getCurrentStroke();	
}

void GrafPlayerApp::updateTransition( int type)
{
	//----------  TRANSITIONS
	// fade away, dissolve, deform etc.
	
	drawer.prelimTransTime += dt;
	
	if(bUseAudio)
	{
	
		float deform_frc = panel.getValueF("wave_deform_force");
		//float line_amp_frc = panel.getValueF("line_width_force");
		float bounce_frc = panel.getValueF("bounce_force");
		
		if( panel.getValueB("use_wave_deform") ) drawer.transitionDeform( dt,deform_frc, audio.audioInput, NUM_BANDS);
		//if( panel.getValueB("use_line_width") ) drawer.transitionLineWidth( dt,audio.averageVal*line_amp_frc);
		if( panel.getValueB("use_bounce") ) drawer.transitionBounce( dt,audio.averageVal*bounce_frc);
		if( drawer.pctTransLine < .1 ) drawer.pctTransLine += .001;
		
	}
	
	
	
	if(bUseArchitecture)
	{
	 
		 if( drawer.prelimTransTime > panel.getValueI("wait_time") )
		 {
			
			bRotating = false;
			
			drawer.transitionFlatten( tags[currentTagID].center.z, 50);
			particleDrawer.flatten( tags[currentTagID].center.z, 52);
			
			if(rotFixTime == 0) rotFixTime = ofGetElapsedTimef();
			float pct = 1 - ((ofGetElapsedTimef()-rotFixTime) / 45.f);
			rotationY = pct*rotationY + (1-pct)*(0);
			
			if( pct < .9 && !archPhysics.bMakingParticles) 
				archPhysics.turnOnParticleBoxes(&particleDrawer.PS);

			if(particleDrawer.xalpha  > 0 ) particleDrawer.xalpha -= .5*dt;
			
			// if all particles have fallen and 
			if(archPhysics.bMadeAll)
			{
				// do average transition
				drawer.transition(dt,.15);
				drawer.alpha -= .35*dt;
				if(particleDrawer.alpha  > 0 ) particleDrawer.alpha -= .5*dt;
			}
			
			//if( bUseGravity ) particleDrawer.fall(dt);
			
		 }
	
	}else{
	
		// do average transition
		//drawer.transition(dt,.99);
		
		drawer.alpha -= .35*dt;
		//if(!bUseAudio) 
		drawer.transition(dt,.15);
		if( bUseGravity ) particleDrawer.fall(dt);
		if(particleDrawer.alpha  > 0 ) particleDrawer.alpha -= .5*dt;
		
	}
	
	//---------- 
}

void GrafPlayerApp::updateAudio()
{
	if(panel.getValueB("use_p_size") ) 
		particleDrawer.updateParticleSizes(audio.eqOutput,audio.averageVal, NUM_BANDS,panel.getValueF("particle_size_force") );
	
	particleDrawer.setDamping( panel.getValueF("P_DAMP") );
	
	if( /*drawer.prelimTransTime < panel.getValueI("wait_time")  &&*/ panel.getValueB("use_p_amp") ) 
		particleDrawer.updateParticleAmpli(audio.ifftOutput,audio.averageVal, NUM_BANDS,panel.getValueF("outward_amp_force") );
	
	// create drops
	if( panel.getValueB("use_drop") )
	{
		
		for( int i = 0; i < audio.peakFades.size(); i++)
		{
			if( audio.peakFades[i] == 1 )
			{
				int randomP = particleDrawer.PS.getIndexOfRandomAliveParticle();//ofRandom( 0, particleDrawer.PS.numParticles );
				ofPoint pPos = ofPoint(particleDrawer.PS.pos[randomP][0],particleDrawer.PS.pos[randomP][1],particleDrawer.PS.pos[randomP][2]);
				ofPoint pVel = ofPoint(particleDrawer.PS.vel[randomP][0],particleDrawer.PS.vel[randomP][1],particleDrawer.PS.vel[randomP][2]);
				drops.createRandomDrop( pPos, pVel, particleDrawer.PS.sizes[randomP] );
			}
		}
		
		// update particle drops (audio stuff);
		drops.update(dt);
	}
	
	// update audio
	audio.update();
}

void GrafPlayerApp::updateArchitecture()
{
	//if( drawer.prelimTransTime < panel.getValueI("wait_time") && !archPhysics.bMakingParticles)
	//	archPhysics.turnOnParticleBoxes(&particleDrawer.PS);
	
	archPhysics.update(dt);
	if(archPhysics.bMakingParticles)
	{
		archPhysics.createParticleSet(&particleDrawer.PS);
	}
	
}

//--------------------------------------------------------------
void GrafPlayerApp::draw(){

	
	// architecture test image
	if( mode == PLAY_MODE_PLAY )
	{
		ofSetColor(150,150,150,255);
		if( bUseArchitecture && panel.getValueB("show_image") )
			archPhysics.drawTestImage();
	}
	
	
	
	//--------- start fbo render
	fbo.clear();
	fbo.begin();
	
	ofEnableAlphaBlending();
	ofSetColor(255,255,255,255);

	if( mode == PLAY_MODE_LOAD )
	{
		//nothing while loading
		;
	}
	else if( mode == PLAY_MODE_RECORD ){
		
		ofPushStyle();
			ofSetColor(255, 255, 255, 255);
			ofNoFill();
			
			for(int k = 0; k < simpleLine.size(); k++){
				if( simpleLine[k].size() < 2 )continue;
				
				ofSetLineWidth(2);
				ofBeginShape();
					for(int i = 0; i < simpleLine[k].size(); i++){
						ofVertex( simpleLine[k].at(i).x * (float)screenW, simpleLine[k].at(i).y * (float)screenH);
					}
				ofEndShape(false);
			}
			
		ofPopStyle();		

		ofSetColor(255, 255, 255, 255);
		
//		if( tags[currentTagID].getNPts() > 3 ){
//		
//			drawer.setup( &tags[currentTagID], tags[currentTagID].distMax );
//							
////			glPushMatrix();
////			
////				if( bUseFog ){
////					glFogf(GL_FOG_START, fogStart );
////					glFogf(GL_FOG_END, fogEnd );
////					glEnable(GL_FOG);				
////				}
////			
////				glPushMatrix();
////					
////					glDisable(GL_DEPTH_TEST);
////									
////					// draw particles
////					particleDrawer.draw(myTagPlayer.getCurrentPoint().z,  screenW,  screenH);
////									
////					// draw audio particles
////					if( bUseAudio && panel.getValueB("use_drop") ) drops.draw();
////					
////					glEnable(GL_DEPTH_TEST);
////					
////					// draw tag
////					glPushMatrix();
////						drawer.draw(  tags[currentTagID].myStrokes.size()-1, tags[currentTagID].getNPts()-1 );
////					glPopMatrix();
////			
////				glPopMatrix();
////			
////			glPopMatrix();
////			
////			glDisable(GL_DEPTH_TEST);
////			glDisable(GL_FOG);
////		
////		}
			
	
	}else if( mode == PLAY_MODE_PLAY && tags.size() ){
		

		glPushMatrix();
		
			if( bUseFog )
			{
				glFogf(GL_FOG_START, fogStart );
				glFogf(GL_FOG_END, fogEnd );
				glEnable(GL_FOG);				
			}

			glTranslatef(screenW/2, screenH/2, 0);
			//glScalef(tags[currentTagID].position.z,tags[currentTagID].position.z,tags[currentTagID].position.z);
			
			glPushMatrix();
		
				glRotatef(tags[currentTagID].rotation.x,1,0,0);
				glRotatef(tags[currentTagID].rotation.y+rotationY,0,1,0);
				glRotatef(tags[currentTagID].rotation.z,0,0,1);
				
				//glTranslatef(-tags[currentTagID].min.x*tags[currentTagID].drawScale,-tags[currentTagID].min.y*tags[currentTagID].drawScale,-tags[currentTagID].min.z);
				glTranslatef(-tags[currentTagID].center.x*tags[currentTagID].drawScale,-tags[currentTagID].center.y*tags[currentTagID].drawScale,-tags[currentTagID].center.z);
		
				glDisable(GL_DEPTH_TEST);
								
				// draw particles
				particleDrawer.draw(myTagPlayer.getCurrentPoint().z,  screenW,  screenH);
								
				// draw audio particles
				if( bUseAudio && panel.getValueB("use_drop") ) drops.draw();
				
				glEnable(GL_DEPTH_TEST);
				
				// draw tag
				glPushMatrix();
					glScalef( tags[currentTagID].drawScale, tags[currentTagID].drawScale, 1);
					drawer.draw( myTagPlayer.getCurrentStroke(), myTagPlayer.getCurrentId() );
				glPopMatrix();
		
			glPopMatrix();
		
		glPopMatrix();
		
		
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_FOG);		
	
	}
	
	if( panel.getValueB("useLaser") && panel.getValueB("bUseClearZone") ){
		laserTracker.drawClearZone(0, 0, screenW, screenH);
	}
	
	if(bUseArchitecture && mode == PLAY_MODE_PLAY )
	{
		
		glViewport(0,0,fbo.texData.width,fbo.texData.height);
		// set translation in polygon tool so drwaing happens in correct place
		archPhysics.offSetPre.x = (tags[currentTagID].position.x);
		archPhysics.offSetPre.y = (tags[currentTagID].position.y);
		archPhysics.offSet.x = (-tags[currentTagID].min.x*tags[currentTagID].drawScale) + (-tags[currentTagID].center.x*tags[currentTagID].drawScale);
		archPhysics.offSet.y = (-tags[currentTagID].min.y*tags[currentTagID].drawScale) + (-tags[currentTagID].center.y*tags[currentTagID].drawScale);
		archPhysics.scale = tags[currentTagID].position.z;
		
		archPhysics.draw();
			
	}
	
	// image mask for edges
	if(bUseMask)
	{
		ofEnableAlphaBlending();
		glBlendFunc(GL_DST_COLOR, GL_ZERO);
		imageMask.draw(0,0,screenW,screenH);
	}
	
	// data
	if( mode == PLAY_MODE_PLAY )
	{
		ofSetColor(255,255,255,255);
		if( bShowName && tags.size() > 0 ) fontL.drawString( tags[ currentTagID ].tagname, 10,ofGetHeight()-30 );
		if( bShowTime && tags.size() > 0 )
		{
			float time = myTagPlayer.getCurrentTime();
			float wd = fontL.stringWidth( ofToString( time,0) ) / 10.f;
			wd = 10*(int)(wd);
			
			fontL.drawString(ofToString(time,2), ofGetWidth()-wd-70, ofGetHeight()-30);
		}
	
	}


	

	// screenshots
	if(bTakeScreenShot)
	{
		imgsaver.grabScreen(0,0,ofGetWidth(),ofGetHeight() );
		
		dirLister.setPath( ofToDataPath(myTagDirectory + tags[currentTagID].tagname + "/", true));
		dirLister.setExtensionToLookFor("png");
		int num = dirLister.getNumberOfFiles();
		imgsaver.saveThreaded( myTagDirectory + tags[currentTagID].tagname + "/"+tags[currentTagID].tagname+"_"+ofToString(num+1)+".png");
		
		bTakeScreenShot = false;
	}
	
	
	//---- end fbo render
	fbo.end();
	
	
	ofEnableAlphaBlending();
	ofSetColor(255,255,255,255);
	
	
	//---- draw fbo to screen
	fbo.drawWarped(0, 0,screenW, screenH,pWarper.u,pWarper.v,pWarper.num_x_sections,pWarper.num_y_sections);
	fbo.drawWarped(screenW, 0,screenW, screenH,pWarper.u,pWarper.v,pWarper.num_x_sections,pWarper.num_y_sections);
	
	
	// -- arch drawing tool
	if(bUseArchitecture)
	{
		if( archPhysics.bDrawingActive || panel.getValueB("show_drawing_tool") )
			archPhysics.drawTool();
	}
	

	//--- control panel
	if(bShowPanel){
		
		if( panel.getCurrentPanelName() == "FBO Warper" )
		{
			if( panel.getValueB("toggle_fbo_preview") )
				pWarper.drawEditInterface(10, 10,.25);
			else 
				ofSetLineWidth(3.0);
				pWarper.drawProjInterface(1024, 0, 1);
				ofSetLineWidth(1.0);
				pWarper.drawEditInterface(0, 0, 1);
			
			ofSetColor(255,255,255,100);
			pWarper.drawUV(0, 0, 1);
		}
		
		if( panel.getCurrentPanelName() == "Laser Tracker" && panel.getValueB("bShowPanel") ){
			laserTracker.drawPanels(0, 0);
		}
		
		panel.draw();
		ofSetColor(255,255,255,200);
		if( panel.getCurrentPanelName() != "Architecture Drawing" )
		{
			fontSS.drawString("x: toggle control panel  |  p: pause/play  |  s: screen capture  |  m: toggle mouse  |  f: toggle fullscreen  |  R: reset pos/rot  |  arrows: next/prev  |  esc: quit", 90, ofGetHeight()-50);
			fontSS.drawString("left mouse: alter position  |  left+shift mouse: zoom  |  right mouse: rotate y  |  right+shift mouse: rotate x", 220, ofGetHeight()-30);
		}else{
			fontSS.drawString("left-mouse: add point  |  right-mouse: move / select all  |  left+shift mouse: move single point  |  Return: new shape  | Delete: remove last point", 90, ofGetHeight()-50);

		}
		
		if( bUseAudio && panel.getCurrentPanelName() == "Audio Settings" )
			audio.draw();
		
	}
	
	ofDrawBitmapString("fps: "+ofToString(ofGetFrameRate()), 10, ofGetHeight()-5);
	
}

//--------------------------------------------------------------
void GrafPlayerApp::keyPressed (ofKeyEventArgs & event){

	if( panel.getValueB("useLaser") ){
	
		if( event.key == '1' ){
			laserTracker.clearZone.setPosition(0, laserTracker.laserX, laserTracker.laserY);
		}
		if( event.key == '2' ){
			laserTracker.clearZone.setPosition(1, laserTracker.laserX, laserTracker.laserY);
		}		
		if( event.key == '3' ){
			laserTracker.clearZone.setPosition(2, laserTracker.laserX, laserTracker.laserY);
		}		
		if( event.key == '4' ){
			laserTracker.clearZone.setPosition(3, laserTracker.laserX, laserTracker.laserY);
		}	

		if( event.key == ' ' ){
			if( mode == PLAY_MODE_PLAY ){
				resetPlayer(0);
			}else{
				panel.setValueB("laserMode", 0);
				panel.setValueB("laserMode", 1);
			}
		}
		
	}
	
		
    switch(event.key){

  		case 'x': bShowPanel=!bShowPanel; break;
		case 'p': 
			myTagPlayer.bPaused = !myTagPlayer.bPaused;
			//bRotating = !myTagPlayer.bPaused;
			panel.setValueB("ROTATE",bRotating);
			panel.setValueB("PLAY",!myTagPlayer.bPaused);
			break;
			
		case OF_KEY_RIGHT:  resetPlayer(1); break;
        case OF_KEY_LEFT:   resetPlayer(-1); break;
		
		case 's': bTakeScreenShot = true; break;
			
		case 'R':
			tags[currentTagID].rotation.set(0,0,0);
			tags[currentTagID].position.set(0,0,1);
			rotationY = 0;
			tagPosVel.set(0,0,0);
			break;
		case OF_KEY_RETURN:
			if( panel.getCurrentPanelName() == "Architecture Drawing" )
				archPhysics.pGroup.addPoly();
			break;
		default:
  			break;

  }

	
	

}

//--------------------------------------------------------------
void GrafPlayerApp::keyReleased(ofKeyEventArgs & event){

}

//--------------------------------------------------------------
void GrafPlayerApp::mouseMoved(ofMouseEventArgs & event ){

	lastX   = event.x;
	lastY   = event.y;
}

//--------------------------------------------------------------
void GrafPlayerApp::mouseDragged(ofMouseEventArgs & event ){

	bool bMouseInPanel = false;
	if( bShowPanel && !panel.minimize) 
	{
		bMouseInPanel = panel.mouseDragged(event.x,event.y,event.button);
		
		if( panel.getCurrentPanelName() == "Laser Tracker" && panel.getValueB("bShowPanel") ){
			if( laserTracker.QUAD.updatePoint(event.x, event.y, 0, 0, 200, 150) ){
				return;
			}
		}
	}
	
	bool bMoveTag = true;
	
	if( bMouseInPanel )	bMoveTag = false;
	else if( panel.getCurrentPanelName() == "Architecture Drawing")	bMoveTag = false;
	else if( pWarper.isEditing() && pWarper.getMouseIndex() != -1)		bMoveTag = false;
	
	if( bMoveTag )
	{
		if( event.button == 0 )
		{
			if(!bShiftOn)
			{
				tagPosVel.x +=  tagMoveForce * (event.x-lastX);
				tagPosVel.y +=  tagMoveForce * (event.y-lastY);
			}else if(tags.size() > 0){
				tags[currentTagID].position.z += .01 * (event.y-lastY);
				tags[currentTagID].position.z = MAX(tags[currentTagID].position.z,.01);
			}
		}else{
			if( tags.size() > 0 && !bShiftOn)		tags[currentTagID].rotation.y += (event.x-lastX);
			else if( tags.size() > 0 && bShiftOn)	tags[currentTagID].rotation.x += (event.y-lastY);
		}
	}	
	
	
	lastX   = event.x;
	lastY   = event.y;

}

//--------------------------------------------------------------
void GrafPlayerApp::mousePressed(ofMouseEventArgs & event ){

	bool bIsMouseInPanel = false;
	
    if( bShowPanel ){
		bIsMouseInPanel = panel.mousePressed(event.x,event.y,event.button);
	
		if( panel.getCurrentPanelName() == "Laser Tracker" && panel.getValueB("bShowPanel") ){
			if( laserTracker.QUAD.selectPoint(event.x, event.y, 0, 0, 200, 150, 15) ){
				return;
			}
		}
	}
	
	if(bUseArchitecture)
	{
		if( bIsMouseInPanel ) archPhysics.pGroup.disableAll(true);
		else if( panel.getCurrentPanelName() == "Architecture Drawing") archPhysics.pGroup.reEnableLast();
	}
	
	
}

//--------------------------------------------------------------
void GrafPlayerApp::mouseReleased(ofMouseEventArgs & event ){

	if( bShowPanel && !panel.minimize) 
	{
		panel.mouseReleased();
	}
	 
	laserTracker.QUAD.releaseAllPoints();
	
}

//--------------------------------------------------------------
void GrafPlayerApp::resetPlayer(int next)
{
	if(tags.size() <= 0 ) return;
	
	myTagPlayer.reset();
	
	nextTag(next);
	
	drawer.setup( &tags[currentTagID], tags[currentTagID].distMax );
	
	particleDrawer.reset();
	
	waitTimer = waitTime;
	
	rotationY = 0;
	tags[currentTagID].rotationSpeed = 0.0;
	tags[currentTagID].rotation = 0.0;
	
	tagPosVel.set(0,0,0);
	
	prevStroke = 0;

	
}
//--------------------------------------------------------------
void GrafPlayerApp::nextTag(int dir)
{
	
	if(dir==1)
	{
		currentTagID++;
		currentTagID %= tags.size();
	}
	else if(dir==-1){
		currentTagID--;
		if(currentTagID < 0 ) currentTagID = tags.size()-1;
	}
		
	drawer.resetTransitions();
	rotFixTime = 0;
	archPhysics.reset();
	
	cout << "total pts this tag " << tags[currentTagID].getNPts() << endl;
	
}
//--------------------------------------------------------------
void GrafPlayerApp::loadTags()
{
	if( tags.size() < totalToLoad )
	{
		tags.push_back( grafTagMulti() );
		int toLoad = tags.size()-1;
		
		gIO.loadTag( filesToLoad[ toLoad ], &tags[ toLoad ]);
		tags[ toLoad ].tagname = filenames[ toLoad ];
		
		
		smoother.smoothTag(4, &tags[ toLoad ]);
		tags[toLoad].average();
		tags[toLoad].average();
		
		
		
	}
	else{
		mode = PLAY_MODE_PLAY;
		resetPlayer(0);
	}
	
}
//--------------------------------------------------------------
void GrafPlayerApp::preLoadTags()
{
	filesToLoad.clear();
	filenames.clear();
	totalToLoad = 0;
	
	cout << "PATH TO TAGS: " << ofToDataPath(myTagDirectory,true) << endl;
	
	vector<string> dirs;
	dirLister.setPath( ofToDataPath(myTagDirectory,true) );
	dirLister.findSubDirectories(dirs);
	
	for( int i = 0; i < dirs.size(); i++)
	{
		dirLister.setPath( ofToDataPath(myTagDirectory+dirs[i]+"/",true) );
		dirLister.setExtensionToLookFor("gml");
		int num = dirLister.getNumberOfFiles();
		if( num > 0 )
		{
			filenames.push_back(dirs[i]);
			filesToLoad.push_back(  ofToDataPath(myTagDirectory+dirs[i]+"/"+dirs[i]+".gml") );
		}
	}
	
	
	dirs.clear();
	dirLister.setPath( ofToDataPath(myTagDirectory,true) );
	dirLister.setExtensionToLookFor( "gml" );
	dirLister.getFileNames(dirs);
	
	for( int i = 0; i < dirs.size(); i++)
	{
		filesToLoad.push_back( ofToDataPath(myTagDirectory+dirs[i]) );
		
		string name = dirs[i];
		int endgml = name.find(".gml");
		if( endgml >= 0 )
			name.erase( endgml,name.size() );
		
		filenames.push_back(name);
	}
	
	totalToLoad = filesToLoad.size();
}

//--------------------------------------------------------------
void GrafPlayerApp::saveTagPositions()
{
	if(currentTagID < 0) return;
	
	ofxXmlSettings xml;
	xml.loadFile(filesToLoad[currentTagID] );
	xml.setValue("GML:tag:environment:offset:x", tags[currentTagID].position.x );
	xml.setValue("GML:tag:environment:offset:y", tags[currentTagID].position.y );
	xml.setValue("GML:tag:environment:offset:z", tags[currentTagID].position.z );
	xml.setValue("GML:tag:environment:rotation:x", tags[currentTagID].rotation.x );
	xml.setValue("GML:tag:environment:rotation:y", tags[currentTagID].rotation.y );
	xml.setValue("GML:tag:environment:rotation:z", tags[currentTagID].rotation.z );
	xml.saveFile(filesToLoad[currentTagID] );
}

//--------------------------------------------------------------
void GrafPlayerApp::setupControlPanel()
{
	
	panel.setup("GA 3.0", ofGetWidth()-320, 20, 300, 600);
	panel.addPanel("App Settings", 1, false);
	panel.addPanel("Draw Settings", 1, false);
	panel.addPanel("Audio Settings", 1, false);
	panel.addPanel("Architecture Drawing", 1, false);
	panel.addPanel("Architecture Settings", 1, false);
	panel.addPanel("FBO Warper", 1, false);
	panel.addPanel("Laser Tracker", 1, false);
	panel.addPanel("Extra Laser Settings", 1, false);

	//---- application sttings
	panel.setWhichPanel("App Settings");
	panel.addToggle("Use Audio", "use_audio",true);
	panel.addToggle("Use Architecture", "use_arch",true);
	panel.addToggle("Play / Pause", "PLAY", true);
	panel.addToggle("FullScreen", "FULL_SCREEN", false);
	panel.addToggle("Rotate", "ROTATE", true);
	panel.addSlider("Rotation Speed","ROT_SPEED",.65,0,4,false);
	panel.addToggle("Display filename", "SHOW_NAME", true);
	panel.addToggle("Display time", "SHOW_TIME", true);
	panel.addToggle("Save Tag Position/Rotation", "save_Tag_pos", false);

	//--- draw settings
	panel.setWhichPanel("Draw Settings");
	panel.addSlider("Line Alpha","LINE_ALPHA",.92,0,1,false);
	panel.addSlider("Outline Width","LINE_WIDTH",2,1,10,false);
	panel.addSlider("Line Scale","LINE_SCALE",.05,0,.1,false);
	panel.addSlider("Particle Size","P_SIZE",2,0,10,false);
	panel.addSlider("Particle Alpha","P_ALPHA",.75,0,1,false);
	panel.addSlider("Particle Damping","P_DAMP",.15,0,.25,false);
	panel.addSlider("Number Particles","P_NUM",1,0,4,true);
	panel.addToggle("Use gravity", "USE_GRAVITY", true);
	panel.addToggle("Use edge mask", "USE_MASK", false);
	
	panel.addToggle("Use Fog", "USE_FOG", false);
	panel.addSlider("Fog Start","FOG_START",fogStart,-2000,2000,true);
	panel.addSlider("Fog End","FOG_END",fogEnd,-2000,2000,true);
	
	
	//--- audio settings
	panel.setWhichPanel("Audio Settings");
	panel.addToggle("Open sound file", "open_sound_file", false);
	panel.addSlider("Outward amp force","outward_amp_force",8,0,200,false);
	panel.addSlider("Particle size force","particle_size_force",22,0,200,false);
	panel.addSlider("Wave deform force","wave_deform_force",.25,0,2,false);
	//panel.addSlider("line width force","line_width_force",.25,0,2,false);
	panel.addSlider("Bounce force","bounce_force",.25,0,2,false);
	panel.addSlider("Change wait time","wait_time",30,0,120,true);
	panel.addSlider("Drop p threshold","drop_p_thresh",.1,0,2,false);
	
	//panel.addSlider("particle speed force","p_audio_damp",1,0,4,false);

	// toggles to apply what to what...
	panel.addToggle("Use particle amp", "use_p_amp", true);
	panel.addToggle("Use particle size", "use_p_size", true);
	//panel.addToggle("use particle speed", "use_p_damp", false);
	panel.addToggle("Use wave deform", "use_wave_deform", true);
	//panel.addToggle("Use line width amp", "use_line_width", false);
	panel.addToggle("Use bounce", "use_bounce", true);
	panel.addToggle("Use drop particel", "use_drop", true);

	panel.setWhichPanel("Architecture Settings");
	panel.addToggle("show drawing tool", "show_drawing_tool",false);
	panel.addToggle("show image", "show_image",true);
	panel.addSlider("mass","box_mass",1,0,20,false);
	panel.addSlider("bounce","box_bounce",.53,0,2,false);
	panel.addSlider("friction","box_friction",.41,0,2,false);
	panel.addSlider("gravity","gravity",6,0,20,false);
	
	panel.setWhichPanel("Architecture Drawing");
	panel.addToggle("new structure", "new_structure",false);
	panel.addToggle("done","arch_done",false);
	panel.addToggle("save xml", "arch_save", false);
	panel.addToggle("load xml", "arch_load", false);
	panel.addToggle("clear", "arch_clear", false);
	
	panel.setWhichPanel("FBO Warper");
	panel.addToggle("Load Warping", "load_warping", true);
	panel.addToggle("Save Warping", "save_warping", false);
	panel.addToggle("Reset Warping", "reset_warping", false);
	panel.addToggle("Toggle Preview","toggle_fbo_preview",false);
	
	panel.setWhichPanel("Laser Tracker");
	panel.addToggle("Use Laser", "useLaser", false);
	panel.addToggle("Video/Camera", "bCamera", false);
	panel.addToggle("camera settings", "camSettings", false);
	panel.addToggle("show panel", "bShowPanel", false);
	panel.addToggle("laser mode", "laserMode", true);
	panel.addSlider("which camera", "whichCamera", 0, 0, 10, true);
	panel.addSlider("Hue", "hue", 0.0, 0, 1.0, false);
	panel.addSlider("Hue Width", "hueWidth", 0, 0, 1.0, false);
	panel.addSlider("Saturation", "saturation", 0, 0, 1.0, false);
	panel.addSlider("Value", "value", 0, 0, 1.0, false);
	panel.addCustomRect("colors tracked", &laserTracker, 140, 40);
	
	panel.setWhichPanel("Extra Laser Settings");
	panel.addToggle("use clear zone", "bUseClearZone", true);
	panel.addToggle("auto end tag rec", "bAutoEndTag", true);
	panel.addSlider("auto end time", "autoEndTime", 2.2, 1.0, 5.0, false);
	panel.addSlider("no. frames new stroke", "nFramesNewStroke", 12, 5, 30, true);
	panel.addSlider("new stroke jump dist", "jumpDist", 0.2, 0.1, 0.99, false);	
	panel.addToggle("laser move", "laserMove", true);
	panel.addSlider("laser rotation amount", "laserRotAmount", 1.0, 0.1, 10.0, false);
	panel.addSlider("laser rotation slowdown", "laserSlowRate", 0.98, 0.88, 0.9999, false);
	
	//--- load saved
	panel.loadSettings(pathToSettings+"appSettings.xml");
	
	panel.setValueB("laserMode", false);
	
	updateControlPanel(true);
	//panel.update();
}
//--------------------------------------------------------------
void GrafPlayerApp::updateControlPanel(bool bUpdateAll)
{
	
	panel.update();
	
	//if(!bUpdateAll && !panel.isMouseInPanel(lastX, lastY) ) return;
	
	if( panel.getCurrentPanelName() == "App Settings" || bUpdateAll)
	{
		
		myTagPlayer.bPaused = !panel.getValueB("PLAY");
		if( panel.getValueB("FULL_SCREEN") )
		{
			panel.setValueB("FULL_SCREEN",false);
			ofToggleFullscreen();
		}
		bRotating = panel.getValueB("ROTATE");
		bShowName = panel.getValueB("SHOW_NAME");
		bShowTime = panel.getValueB("SHOW_TIME");
		
		if( panel.getValueB("save_Tag_pos") )
		{
			panel.setValueB("save_Tag_pos",false);
			saveTagPositions();
		}


	}
	
	if( panel.getCurrentPanelName() == "Draw Settings" || bUpdateAll)
	{
	
		drawer.setAlpha(panel.getValueF("LINE_ALPHA"));
		drawer.lineWidth = panel.getValueF("LINE_WIDTH");
		drawer.setLineScale( panel.getValueF("LINE_SCALE") );
	
		particleDrawer.setParticleSize( panel.getValueF("P_SIZE") );
		particleDrawer.particle_alpha = panel.getValueF("P_ALPHA") ;
		particleDrawer.numXtras = panel.getValueI("P_NUM");
		bUseGravity = panel.getValueB("USE_GRAVITY");
		bUseMask = panel.getValueB("USE_MASK");
		
		
		bUseFog = panel.getValueB("USE_FOG");
		fogStart = panel.getValueI("FOG_START");
		fogEnd = panel.getValueI("FOG_END");
		
		
	}
	
	if(panel.getCurrentPanelName() == "Audio Settings" )
	{
	
		audio.peakThreshold = panel.getValueF("drop_p_thresh");
		
		if( panel.getValueB("open_sound_file") )
		{
				panel.setValueB("open_sound_file", false); 
					
				char msg[] = {"please select a file"};
				char msg2[] = {""};
					
				string result = dialog.getStringFromDialog(kDialogFile, msg, msg2);					
				vector <string> checkDot = ofSplitString(result, ".");
					
				if(  checkDot.back() == "mp3" || checkDot.back() != "wav"  )
				{
					audio.music.loadSound(result);
					//audio.music.play();
				}
		}
	
	
	}
	
	if( panel.getCurrentPanelName() == "Architecture Drawing" )
	{
		if(panel.newPanelSelected())
		{
			archPhysics.bDrawingActive = true;
			archPhysics.pGroup.reEnableLast();
		}
		
		if( panel.getValueB("arch_done") )
		{
			panel.setValueB("arch_done",false);
			createWarpedArchitecture();
		}
		
		if( panel.getValueB("new_structure") )
		{
			panel.setValueB("new_structure",false);
			archPhysics.pGroup.addPoly();
		}
		
		if( panel.getValueB("arch_save") )
		{
			panel.setValueB("arch_save",false);
			archPhysics.saveToXML(pathToSettings+"architecture.xml");
		}
		
		if( panel.getValueB("arch_load") )
		{
			panel.setValueB("arch_load",false);
			archPhysics.loadFromXML(pathToSettings+"architecture.xml");
			createWarpedArchitecture();
		}
		
		if( panel.getValueB("arch_clear") )
		{
			panel.setValueB("arch_clear",false);
			archPhysics.pGroup.clear();
			archPhysics.pGroup.addPoly();
		}
	}
	
	if( panel.getCurrentPanelName() == "Architecture Settings" )
	{
		archPhysics.bShowArchitecture = panel.getValueB("show_drawing_tool");
		
		if( panel.hasValueChangedInPanel("Architecture Settings") ){
			archPhysics.setPhysicsParams( panel.getValueF("box_mass"), panel.getValueF("box_bounce"), panel.getValueF("box_friction"));
			archPhysics.box2d.setGravity(0, panel.getValueI("gravity") );
		}
	}
	
	if( panel.getCurrentPanelName() == "FBO Warper")
	{
		pWarper.enableEditing();
		
		if( panel.getValueB("load_warping") )
		{
			panel.setValueB("load_warping",false);
			pWarper.loadFromXml(pathToSettings+"warper.xml");
		}
		
		if( panel.getValueB("save_warping") )
		{
			panel.setValueB("save_warping",false);
			pWarper.saveToXml(pathToSettings+"warper.xml");
		}
		
		if( panel.getValueB("reset_warping") )
		{
			panel.setValueB("reset_warping",false);
			pWarper.initWarp( screenW,screenH,screenW*WARP_DIV,screenH*WARP_DIV );
			pWarper.recalculateWarp();
		}
		
		
		
	}
	

	//--- disable things
	if( panel.getCurrentPanelName() != "FBO Warper" )
	{
		pWarper.disableEditing();
	}
	
	if(panel.getCurrentPanelName() != "Architecture Drawing")
	{
		archPhysics.bDrawingActive = false;
		archPhysics.pGroup.disableAll(true);
	}
	
	
	bUseAudio = panel.getValueB("use_audio");
	bUseArchitecture = panel.getValueB("use_arch");
	
	if(bUseArchitecture && !archPhysics.bSetup )
		archPhysics.setup(screenW,screenH);
		
	
}

//--------
bool GrafPlayerApp::checkLaserActive(){
	if( panel.hasValueChanged("useLaser") || panel.hasValueChanged("bCamera") ){
		if( panel.getValueB("bCamera") ){
			laserTracker.setupCamera( panel.getValueI("whichCamera"), 320, 240);
		}else{
			laserTracker.setupVideo("laserTestVideo.mov");
		}
	}
	
	if( panel.getValueB("bCamera") && panel.getValueB("camSettings") ){
		panel.setValueB("camSettings", false);
		laserTracker.openCameraSettings();
	}
	
	return panel.getValueB("useLaser");
}

//--------
void GrafPlayerApp::handleLaserPlayback(){
	laserTracker.clearNewStroke();
	
	if( !checkLaserActive() ){
		return;
	}
		
	if( panel.hasValueChanged("laserMode") ){
		resetPlayer(0);
		tags[currentTagID].center.x  = 0.5;
		tags[currentTagID].center.y  = 0.5;
		tags[currentTagID].drawScale = screenH;
		
		drawer.transitionFlatten( tags[currentTagID].center.z, 1);
		particleDrawer.flatten( tags[currentTagID].center.z, 1);		
	}

	float preX = laserTracker.laserX;
	float preY = laserTracker.laserY;
	
	laserTracker.processFrame(panel.getValueF("hue"), panel.getValueF("hueWidth"), panel.getValueF("saturation"), panel.getValueF("value"), 2, 12, 0.1, true);

	float diff = laserTracker.laserX - preX;
		
	if( panel.getValueB("laserMove") && tags.size() ){
		
		//printf("tags rotation is %f \n", tags[currentTagID].rotation.x);
		
		tags[currentTagID].rotationSpeed.y	*= panel.getValueF("laserSlowRate");
		
		if( laserTracker.newData() ){
			if( laserTracker.isStrokeNew() ){
				preX = laserTracker.laserX;
				preY = laserTracker.laserY;
				diff = 0.0;
				printf("stroke is new!\n");
			}
			tags[currentTagID].rotationSpeed.y	+= diff * panel.getValueF("laserRotAmount");
		}

		tags[currentTagID].rotation.y		+= tags[currentTagID].rotationSpeed.y;

	}
	
}

//--------
void GrafPlayerApp::handleLaserRecord(){

	laserTracker.clearNewStroke();

	if( !checkLaserActive() ){
		return;
	}
		
	bool newTag = ( panel.hasValueChanged("laserMode") && panel.getValueB("laserMode") );
	
	if( newTag ){
		particleDrawer.reset();
		printf("clearing tags\n");
		//tags.clear();
	}
	
	if( tags.size() ==  0 ){
		tags.push_back(grafTagMulti());
	}
	
	currentTagID = tags.size()-1;
		
	if( newTag ){
		tags.back().clear(true);
		simpleLine.clear();
		simpleLine.push_back( simpleStroke() );		
	}
	
	bool bNewStroke = false;
	float preX		= laserTracker.laserX;
	float preY		= laserTracker.laserY;	
	
	laserTracker.processFrame(panel.getValueF("hue"), panel.getValueF("hueWidth"), panel.getValueF("saturation"), panel.getValueF("value"), 2, panel.getValueI("nFramesNewStroke"), panel.getValueF("jumpDist"), true);

	if( laserTracker.newData() && laserTracker.isStrokeNew() && !newTag ){
	
		//tags.back().nextStroke();
		printf("stroke is new!\n");
		
		if( tags.back().getNPts() == 0 ){
			tags.back().clear(true);
			simpleLine.clear();
			simpleLine.push_back( simpleStroke() );		
		}

		preX = laserTracker.laserX;
		preY = laserTracker.laserY;
		bNewStroke = true;
	}
	
	if( laserTracker.newData() ){
	
		if( simpleLine.size() == 0 ){
			simpleLine.push_back( simpleStroke() );
		}
	
		if( bNewStroke ){		
			smoothX = preX = laserTracker.laserX;
			smoothY = preY = laserTracker.laserY;
			
			tags.back().nextStroke();
			tags.back().addNewPoint(ofPoint(smoothX, smoothY) );
			
			simpleLine.push_back( simpleStroke() );
			simpleLine.back().push_back(ofPoint(smoothX, smoothY) );

		}else{
			for(int k = 0; k < 4; k++){
				smoothX *= 0.6;
				smoothY *= 0.6;
				
				float ptX = ofMap(k+1, 0, 4, preX, laserTracker.laserX);
				float ptY = ofMap(k+1, 0, 4, preY, laserTracker.laserY);

				smoothX += ptX * 0.4;
				smoothY += ptY * 0.4;
								
				tags.back().addNewPoint(ofPoint(smoothX, smoothY));
				simpleLine.back().push_back( ofPoint(smoothX, smoothY) );
			}
		}
		
		lastTimePointAddedF = ofGetElapsedTimef();
	}else{
		
		if( tags.back().getNPts() > 3 && panel.getValueB("bAutoEndTag") && ofGetElapsedTimef() - lastTimePointAddedF > panel.getValueF("autoEndTime")){
			panel.setValueB("laserMode", false);
			laserTracker.clearZone.bState = false;
			resetPlayer(0);
			tags[currentTagID].center.x  = 0.5;
			tags[currentTagID].center.y  = 0.5;
			tags[currentTagID].drawScale = screenH;
			
			drawer.transitionFlatten( tags[currentTagID].center.z, 1);
			particleDrawer.flatten( tags[currentTagID].center.z, 1);
		}
	
	}
	
}

//------------------------------------------
void GrafPlayerApp::checkLaserHitState(){
	if( laserTracker.clearZone.bState != panel.getValueB("laserMode") ){
		panel.setValueB("laserMode", laserTracker.clearZone.bState );
	}
}

string GrafPlayerApp::getCurrentTagName()
{
	if(tags.size() <= 0 ) return " ";
	else if( mode == PLAY_MODE_LOAD) return tags[tags.size()-1].tagname;
	else return tags[currentTagID].tagname;
}


void GrafPlayerApp::createWarpedArchitecture()
{
	wPolys.clear();
	
	for( int i = 0; i < archPhysics.pGroup.polys.size(); i++)
	{
		polySimple tPoly;
		tPoly.pts.assign( archPhysics.pGroup.polys[i]->pts.begin(),archPhysics.pGroup.polys[i]->pts.end() );
		wPolys.push_back(tPoly);
	}
	
	for( int i = 0; i < wPolys.size(); i++)
	{
		for( int j = 0; j < wPolys[i].pts.size(); j++)
		{
			ofPoint wPoint = pWarper.warpPoint(wPolys[i].pts[j]);
			wPolys[i].pts[j] = wPoint;
		}
	}
	
	archPhysics.createArchitectureFromPolys(wPolys);
}


void GrafPlayerApp::loadScreenSettings()
{
	ofxXmlSettings xml;
	xml.loadFile(pathToSettings+"screenSettings.xml");
	xml.pushTag("screen");
		screenW = xml.getValue("width",1024);
		screenH = xml.getValue("height",768);
	xml.popTag();
}



