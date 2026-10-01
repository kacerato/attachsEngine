// Quaternion error spring. Offsets remain authored as Euler degrees, but
// evolution and shortest-path selection never depend on Euler decomposition.
static void qMultiply(const float a[4],const float b[4],float out[4]) {
 float r[4]{a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
 double length=0;for(auto n:r)length+=double(n)*n;length=std::sqrt(length);for(u32 i=0;i<4;++i)out[i]=static_cast<float>(r[i]/length);
}
static void qDifference(const float a[4],const float b[4],double out[3]) {
 float inverse[4]{-b[0],-b[1],-b[2],b[3]},q[4];qMultiply(a,inverse,q);if(q[3]<0)for(auto &n:q)n=-n;
 const double length=std::sqrt(double(q[0])*q[0]+double(q[1])*q[1]+double(q[2])*q[2]);
 const double factor=length>1e-10?2*std::atan2(length,double(q[3]))/length:2;
 for(u32 a=0;a<3;++a)out[a]=q[a]*factor;
}
static void qApply(const double delta[3],const float original[4],float out[4]) {
 const double length=std::sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
 const double factor=length>1e-10?std::sin(length*.5)/length:.5;
 float q[4]{float(delta[0]*factor),float(delta[1]*factor),float(delta[2]*factor),float(std::cos(length*.5))};qMultiply(q,original,out);
}
bool applySpringRotation(ObjectId id,const scene::SpringRotationConstraint &c,const Transform &source,const Transform &initial,Transform &output,double dt) {
 auto &state=springs_[{id,c.instanceId()}];
 if(!state.initialized||state.target!=c.target){transformRotationQuaternion(initial,state.quaternion);std::fill(std::begin(state.velocity),std::end(state.velocity),0);state.target=c.target;state.initialized=true;}
 float sourceQ[4],offsetQ[4],desired[4],rest[4],goal[4];transformRotationQuaternion(source,sourceQ);
 Transform offset;std::copy(std::begin(c.offset),std::end(c.offset),offset.rotationDegrees);transformRotationQuaternion(offset,offsetQ);qMultiply(sourceQ,offsetQ,desired);transformRotationQuaternion(output,rest);
 double influence[3];qDifference(desired,rest,influence);for(auto &n:influence)n*=c.weight;qApply(influence,rest,goal);
 double error[3],nextVelocity[3];qDifference(state.quaternion,goal,error);
 for(u32 a=0;a<3;++a){nextVelocity[a]=state.velocity[a];springStep(error[a],nextVelocity[a],6.283185307179586*c.frequency,c.dampingRatio,dt);}
 float next[4];qApply(error,goal,next);double movement[3];qDifference(next,state.quaternion,movement);
 const double angle=std::sqrt(movement[0]*movement[0]+movement[1]*movement[1]+movement[2]*movement[2]);
 const double ratio=angle>0?std::min(1.0,c.maxSpeed*.017453292519943295*dt/angle):1;
 for(u32 a=0;a<3;++a){movement[a]*=ratio;state.velocity[a]=float(nextVelocity[a]*ratio);}qApply(movement,state.quaternion,state.quaternion);
 const auto *q=state.quaternion;const float x=q[0],y=q[1],z=q[2],w=q[3];
 float matrix[16]{1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y),0,2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x),0,2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y),0,0,0,0,1},identity[16]{};for(u32 i=0;i<4;++i)identity[i*5]=1;
 Transform rotation;if(!localTransformForWorld(matrix,identity,rotation))return false;
 std::copy(std::begin(rotation.rotationDegrees),std::end(rotation.rotationDegrees),output.rotationDegrees);return true;
}
