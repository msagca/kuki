# The duel

A one-on-one fight over the network (`NetworkSystem`: `--host`, `--join`; `--lag`/`--jitter`/
`--loss` stand one PC in for the internet), using the tick, gestures, the stepped animator and IK.

It opens on a select screen (`DuelSystem`'s `Screen`: A/D choose, Enter fights), which is what an
unattended `--capture` shows. The settings' game page leaves a match, picks the stage, and turns off
the fighters' watching (`--no-ik`) and the screen flashes (`--no-flash`); a flag wins over what was
saved, for that run.

The four fighters come from `tools/make_fighter.py`: one rig, with clips shared by every armature so
any two can fight, and an armature in a .blend's `Export` collection exported with its skin and clips.
The four stages (alley, temple, rooftop, shrine) come from `tools/make_stages.py` into `stages.blend`
and are dressed in `stage.cpp`; `StageSystem` builds the one `--stage` names, switches on Tab or the
game page, and animates the lights and lightning. Both scripts are run by hand and their .blend files
committed.

The rules (`duel_sim.cpp`) are windowless and compiled into the tests.
