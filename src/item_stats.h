#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
// Item numbers the combat code reads, copied out of an objdb record's chunks. A chunk is {u32 tag, u32 size
// counting this 8-byte header, payload}. Formulas are the handling-block builders 0x165ad0/0x165420 and the
// layer build 0xeca40 as traced statically; rdb.weapon_strike/armour compute the same.
namespace scripting {
struct Grip {unsigned kind=0;double edgeStart=0,edgeEnd=0,cut=0,crush=0,point=0;};
struct WeaponStats {
 uint32_t flags=0;unsigned sound=0;std::array<uint8_t,18> raw{};
 double heft=0,balance=0,effectiveHeft=0,thrust=0,parryWeight=0,powerMultiplier=1,recoil=0,recoilTwoHanded=0;
 Grip grips[2];
};
struct ArmourStats {
 std::array<uint32_t,18> coverage{};double impact=0,cut=0,crush=0,pierce=0;unsigned encumbrance=0,layer=0,vision=0;
};
// Condition bytes 0..255 the drop and merchant code write and the repair jobs lower: weapon payload +0x0c/+0x0d/
// +0x0e, apparel +0x1b0/+0x1b1/+0x1b2 (quality, wear, dirt).
struct Condition {bool known=false;unsigned quality=0,wear=0,dirt=0;};
struct ItemStats {bool weapon=false,armour=false;WeaponStats w;ArmourStats a;Condition condition;};
inline constexpr uint32_t weaponTag=0x4000,apparelTag=0x1000;
inline constexpr size_t weaponBytes=0x42,apparelBytes=0x1b3;   // payload bytes read
inline constexpr const char* bodyParts[18]={"pelvis","right_thigh","right_shin","right_foot","left_thigh","left_shin","left_foot",
 "belly","chest","head","right_shoulder","right_upper_arm","right_forearm","left_shoulder","left_upper_arm","left_forearm","right_hand","left_hand"};
inline double strikePower(uint8_t b){return b?0.5*std::pow(b*0.01,0.7):0.0;}
// 0x165420 with charge 0: two hands make the felt heft (2-heft)*heft and the balance 0.7*balance.
inline double recoilOf(double heft,double balance){return 0.1+0.4*heft+(balance<=1?0.6*(1-balance):0.0);}
inline Condition conditionAt(const uint8_t* p){return {true,p[0],p[1],p[2]};}
inline void parseWeapon(const uint8_t* p,WeaponStats& w){
 std::memcpy(&w.flags,p,4);w.flags&=0x603ffff;std::memcpy(w.raw.data(),p+0x30,w.raw.size());
 w.heft=p[0x30]*0.01;w.balance=p[0x31]*0.01;
 w.effectiveHeft=0.1+0.4*w.heft+(w.balance<=1?0.6*(1-w.balance):0.0)+(int(p[0x32])-127)*0.01;
 w.thrust=strikePower(p[0x33]);w.parryWeight=0.8-((w.heft+1)-w.balance)*0.125;w.powerMultiplier=1+(int(p[0x40])-128)*0.00156;w.sound=p[0x41];
 w.recoil=recoilOf(w.heft,w.balance);w.recoilTwoHanded=recoilOf((2-w.heft)*w.heft,0.7*w.balance);
 for(int g=0;g<2;++g){const uint8_t* q=p+0x34+6*g;auto& grip=w.grips[g];
  grip.kind=q[0];grip.edgeStart=q[1]*0.005;grip.edgeEnd=q[2]*0.005;grip.cut=strikePower(q[3]);grip.crush=strikePower(q[4]);grip.point=strikePower(q[5]);}
}
// Coverage is a u32 percent per body part from payload +0x150; protection bytes from +0x1a0.
inline void parseArmour(const uint8_t* p,ArmourStats& a){
 for(size_t i=0;i<a.coverage.size();++i)std::memcpy(&a.coverage[i],p+0x150+4*i,4);
 a.impact=p[0x1a0]*0.005;a.cut=p[0x1a1]/140.0;a.crush=p[0x1a2]/140.0;a.pierce=p[0x1a3]/140.0;a.encumbrance=p[0x1a5];a.layer=p[0x1a6];a.vision=p[0x1a7];
}
}
