#include "ofApp.h"
#include "ofxPd.h"

//--------------------------------------------------------------
void ofApp::setup(){
    players.emplace_back(ofColor::green, 16, *this);
    players.emplace_back(ofColor::orange, 32, *this );
    players.emplace_back(ofColor::red, 48, *this);
    players.emplace_back(ofColor::blue, 96, *this);
    
    
    ofBackground(ofColor::white);
    vector<ofVideoDevice> devices = videoCamera.listDevices();
    
    for(int i = 0; i < devices.size(); i++){
        if(devices[i].bAvailable){
            ofLogNotice() << devices[i].id << ": " << devices[i].deviceName;
        }else{
            ofLogNotice() << devices[i].id << ": " << devices[i].deviceName << " - unavailable ";
        }
    }
    
    videoCamera.setDeviceID(0);
    videoCamera.initGrabber(ofGetWidth(), ofGetHeight());
    
    colorImage.allocate(ofGetWidth(), ofGetHeight());
    grayImage.allocate(ofGetWidth(), ofGetHeight());
    score.allocate(kWidth, kHeight, OF_PIXELS_GRAY);

    
    score.setColor(ofColor::white);
    scoreTexture.allocate(score);

    ofSetVerticalSync(true);
    font.load("Arial.ttf", 20);
    waitTime = 10;
    
    
    int ticksPerBuffer = 8; // 8 * 64 = buffer len of 512
    
    ofSoundStreamSetup(2, 0, this, 44100, ofxPd::blockSize()*ticksPerBuffer, 3);
    if(!pd.init(2, 0, 44100, ticksPerBuffer, false)) {
        OF_EXIT_APP(1);
    }

    
    pd.addReceiver(*this);
    pd.subscribe("toOF");
    pd.start();
    pd.openPatch("sonify.pd");
    mode = Mode::wait;
    
}


ofPixels &ofApp::getScore(){
    return score;
}

void ofApp::addCollision(ofPoint position, Player &player){
    collisions.emplace_front(position,player);
}

#pragma mark update

void ofApp::scoreAnalysis(){
    grayImage.contrastStretch();
    ofPixels & pixels = grayImage.getPixels();
    for(int y = 0; y < ofGetHeight()-1; y++){
        int offset = y * ofGetWidth();
        for(int x = 0; x < ofGetWidth(); x++){
            int hoffset = offset+x;
            if(abs(pixels[hoffset+1] - pixels[hoffset]) > 10 ||
               abs(pixels[hoffset+ofGetWidth()] - pixels[hoffset]) > 10){
                score[hoffset]  = (abs(pixels[hoffset+1] - pixels[hoffset]) + abs(pixels[hoffset+ofGetWidth()] - pixels[hoffset])) * 10 ;
            }else{
                score[hoffset] = 255;
            }
        }
        
    }
    
    scoreTexture.loadData(score);
    const unsigned char * data = score.getData();
    imageData.clear();
    for(int i = 0 ; i < score.size(); i++){
        imageData.push_back(static_cast<float>(255-data[i]));
    }
    pd.writeArray("imageData", imageData);
    
    
}


void ofApp::update(){
    
    switch(mode){
        case Mode::wait:{
            
            videoCamera.update();
            colorImage = videoCamera.getPixels();
            grayImage = colorImage;
            break;
        }
        case Mode::capture:{
            scoreAnalysis();
            
            
            mode = Mode::transition;
            alpha = 0.0;
            break;
        }
        case Mode::transition:{
            alpha += 0.01;
            if(alpha > 1){//fade in from white (shutter effect)
                mode = Mode::sonification;
                pd.sendMessage("fromOF", "start");
            }
            break;
        }
        case Mode::sonification:{
            for(auto &player:players){player.update();}
            for(auto &collision: collisions){collision.update();}
            collisions.remove_if([](Collision &collision){ return collision.getDead();});
            break;
        }
    }
}

#pragma mark draw

void ofApp::drawTextRegion(){
    ofSetColor(ofColor(0,0,200,100));
    ofFill();
    ofDrawRectangle(0, ofGetHeight()/2 - 50, ofGetWidth(), 100);
}

void ofApp::drawMatrix(){
    ofSetLineWidth(0.5);
    ofSetColor(ofColor::black);
    int offset = 64;
    for(int i = 0; i < 16; i++){
        ofDrawLine(offset, 0, offset, ofGetHeight());
        offset += 64;
    }
    offset = 64;
    for(int i = 0; i < 12; i++){
        ofDrawLine(0, offset, ofGetWidth(), offset);
        offset += 64;
    }
}


void ofApp::drawPlayer(){
    for(auto &player:players){player.draw();}
}

void ofApp::drawCollisions(){
    for(auto &collision:collisions){collision.draw();}
}

void ofApp::drawMessage(){
    ofSetColor(ofColor::white);
    std::string message = ofToString(waitTime);
    float width = font.stringWidth(message);
    font.drawString(message, (ofGetWidth() - width) / 2, ofGetHeight()/2);
}

void ofApp::draw(){
    switch (mode) {
        case Mode::wait:{
            ofSetColor(ofColor::white);
            grayImage.draw(0, 0);
            drawTextRegion();
            drawMessage();
            break;
        }
        case Mode::capture:{
            break;
        }
        case Mode::transition:{
            ofSetColor(ofFloatColor(1,1,1,alpha));
            scoreTexture.draw(0, 0);
            break;
        }
        case Mode::sonification:{
            ofSetColor(ofFloatColor(1,1,1,alpha));
            scoreTexture.draw(0, 0);
            drawPlayer();
            drawCollisions();
            break;
        }
        default:
            break;
    }
}

#pragma mark Pd
void ofApp::receiveMessage(const std::string &dest, const std::string &msg, const pd::List &list){
    
    if(msg == "waitTime"){
        waitTime = static_cast<int>(list.getFloat(0));
        if(waitTime == 0){ mode = Mode::capture; }
    }else if(msg == "player"){
        int id = static_cast<int>(list.getFloat(0));
        std::string descriptor = list.getSymbol(1);
        int value = static_cast<int>(list.getFloat(2));
        if(descriptor == "position"){
            players[id].setPositionFromIndex(value);
        }else if(descriptor == "direction"){
            players[id].setDirection(static_cast<bool>(value));
        }else if(descriptor == "active"){
            players[id].setActive(static_cast<bool>(value));
        }
    }
}

void ofApp::print(const std::string &msg){
    ofLog() << msg;
}

void ofApp::keyPressed(int key){

}
void ofApp::keyReleased(int key){
    
}

//--------------------------------------------------------------
void ofApp::audioIn(float * input, int bufferSize, int nChannels) {
    pd.audioIn(input, bufferSize, nChannels);
}

//--------------------------------------------------------------
void ofApp::audioOut(float * output, int bufferSize, int nChannels) {
    pd.audioOut(output, bufferSize, nChannels);
}