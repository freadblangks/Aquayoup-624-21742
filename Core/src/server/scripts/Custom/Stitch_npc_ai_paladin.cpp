////#########################################################################################################################################################################################################################################
// Copyright (C) Juin 2020 Stitch pour Aquayoup
// AI generique npc par classe : PALADIN V1.0
// Il est possible d'influencer le temp entre 2 cast avec `BaseAttackTime` & `RangeAttackTime` 
// Necessite dans Creature_Template :
// Minimun  : UPDATE `creature_template` SET `ScriptName` = 'Stitch_npc_ai_paladin',`AIName` = '' WHERE (entry = 15100006);
// Optionel : UPDATE `creature_template` SET `HealthModifier` = 2, `ManaModifier` = 3, `ArmorModifier` = 1, `DamageModifier` = 2,`BaseAttackTime` = 2000, `RangeAttackTime` = 2000 WHERE(entry = 15100006);
// Optionel : Utilisez pickpocketloot de creature_template pour passer certains parametres (Solution choisit afin de rester compatible avec tout les cores). Si pickpocketloot = 1 (branche1 forc�), pickpocketloot = 2 (branche2 forc�), etc
//###########################################################################################################################################################################################################################################
// # npc de Test Stitch_npc_ai_paladin  .npc 15100006
// REPLACE INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `modelid1`, `modelid2`, `modelid3`, `modelid4`, `name`, `femaleName`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `exp_unk`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `scale`, `rank`, `dmgschool`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `trainer_type`, `trainer_class`, `trainer_race`, `type`, `type_flags`, `type_flags2`, `lootid`, `pickpocketloot`, `skinloot`, `resistance1`, `resistance2`, `resistance3`, `resistance4`, `resistance5`, `resistance6`, `spell1`, `spell2`, `spell3`, `spell4`, `spell5`, `spell6`, `spell7`, `spell8`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `InhabitType`, `HoverHeight`, `HealthModifier`, `HealthModifierExtra`, `ManaModifier`, `ManaModifierExtra`, `ArmorModifier`, `DamageModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `mechanic_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
// (15100006, 0, 0, 0, 0, 0, 29491, 0, 0, 0, 'npc_ai_Paladin', '', '', '', 0, 90, 90, 0, 0, 2102, 0, 1.01, 1.01, 1, 0, 0, 2000, 2000, 1, 1, 2, 0, 2048, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 3, 1, 2, 1, 1.5, 1, 2, 2, 1, 0, 144, 1, 0, 0, 'Stitch_npc_ai_paladin', 20173);
// REPLACE INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`) VALUES
// (15100006, 1, 854, 0, 0, 0),				// baton			- Restauration
// (15100006, 2, 2209, 63165, 0, 0),		// dague+bouclier	- Elementaire
// (15100006, 3, 2209, 2209, 0, 0);		    // dague * 2		- Amelioration
//###########################################################################################################################################################################################################################################



//################################################################################################
//StitchAI AI Paladin
//################################################################################################

class Stitch_npc_ai_paladin : public CreatureScript
{
public: Stitch_npc_ai_paladin() : CreatureScript("Stitch_npc_ai_paladin") { }

		struct Stitch_npc_ai_paladinAI : public ScriptedAI
		{
			Stitch_npc_ai_paladinAI(Creature* creature) : ScriptedAI(creature) { }

			uint32 BrancheSpe = 1;													// Choix de la Sp�cialisation : Vindice=1, Sacr�=2, Protection=3
			uint32 NbrDeSpe = 3;													// Nombre de Sp�cialisations
			uint32 ForceBranche;
			uint32 Random;
			uint32 DistanceDeCast = 30;												// Distance max a laquelle un npc attaquera , au dela il quite le combat
			uint32 ResteADistance = 5;												// Distance max a laquelle un npc s'approchera
			uint32 Dist;															// Distance entre le npc et sa cible
			uint32 Mana;
			uint32 MaxMana = me->GetMaxPower(POWER_MANA);
			Unit* victim = me->GetVictim();										 
			
			// Definitions des variables Cooldown et le 1er lancement
			uint32 Cooldown_Spell1 = 500;
			uint32 Cooldown_Spell2 = 2000;
			uint32 Cooldown_Spell3 = 3500;
			uint32 Cooldown_Spell_Heal = 3000;
			uint32 Cooldown_RegenMana = 3000;
			uint32 Cooldown_ResteADistance = 4000;									// Test si en contact
			uint32 Cooldown_ResteAuContact;
			uint32 Cooldown_Anti_Bug_Figer = 2000;
			uint32 Cooldown_Npc_Emotes = urand(5000, 8000);

			// Spells Divers
			uint32 Buf_branche1 = 20164;											// Sceau de justice 20164, Sceau de clairvoyance 20165, Sceau de v�rit� 30801, Sceau de m�ditation 105361
			uint32 Buf_branche1a = 79977;											// B�n�diction de puissance 79977
			uint32 Buf_branche2 = 20165;											// Sceau de clairvoyance 20165, Sceau de protection de mana 20154,
			uint32 Buf_branche2a = 31821;											// Aura de d�votion 31821, Aura de d�votion 52442 (armure+25%)
			uint32 Buf_branche3 = 30801;											// Sceau de clairvoyance 20165, Sceau de v�rit� 30801
			uint32 Buf_branche3a = 31850;											// Ardent d�fenseur 31850 (dmg -20% 10s),     B�n�diction des rois 72043, Protection divine 498, Aura de d�votion 52442 (armure+25%) 
			uint32 Spell_Heal_Caster = 19750;  										// Eclair lumineux 19750
			uint32 Spell_Heal_Heal = 19750;  										// Lumi�re sacr�e 82326

			// Spells Vindice
			uint32 Spell_branche1_agro;
			uint32 Spell_branche1_1;
			uint32 Spell_branche1_2;
			uint32 Spell_branche1_3;
			uint32 branche1_agro[4] = { 853, 853, 96231, 62124 };					// Marteau de la justice 853 (stun 6s), R�primandes 96231 (interrompt 4s), R�tribution 62124
			uint32 branche1_1[2] = { 20271, 20271 };								// Jugement 20271
			uint32 branche1_2[2] = { 35395, 35395 };								// Frappe du crois� 35395
			uint32 branche1_3[3] = { 879, 53595, 53385 };							// Exorcisme 879, Marteau du vertueux 53595 6s, Temp�te divine 53385
			
			// Spells Sacr�
			uint32 Spell_branche2_agro;
			uint32 Spell_branche2_1;
			uint32 Spell_branche2_2;
			uint32 Spell_branche2_3;
			uint32 branche2_agro[4] = { 2812, 2812, 853, 96231 };					// D�noncer 2812 , Marteau de la justice 853 (stun 6s), R�primandes 96231 (interrompt 4s)
			uint32 branche2_1[2] = { 20271, 20271 };								// Jugement 20271
			uint32 branche2_2[2] = { 35395, 35395 };								// Frappe du crois� 35395
			uint32 branche2_3[2] = { 20473, 20473 };								// Horion sacr� 20473

			// Spells Protection
			uint32 Spell_branche3_agro;
			uint32 Spell_branche3_1;
			uint32 Spell_branche3_2;
			uint32 Spell_branche3_3;
			uint32 branche3_agro[3] = { 853, 96231, 62124 };						// Marteau de la justice 853 (stun 6s), R�primandes 96231 (interrompt 4s), R�tribution 62124
			uint32 branche3_1[2] = { 20271, 20271 };								// Jugement 20271
			uint32 branche3_2[2] = { 53600, 53600 };								// Frappe du crois� 35395, Bouclier du vertueux 53600
			uint32 branche3_3[2] = { 31935, 26573 };								// Bouclier du vengeur 31935 (interrompt 3s), Cons�cration 26573, 

			// Emotes
			uint32 Npc_Emotes[22] = { 1,3,7,11,15,16,19,21,22,23,24,53,66,71,70,153,254,274,381,401,462,482 };

			void JustRespawned() override
				{
				me->GetMotionMaster()->MoveTargetedHome();								// Retour home pour rafraichir client
				me->SetSpeedRate(MOVE_RUN, 1.01f);
				me->SetReactState(REACT_AGGRESSIVE);
				}
			void EnterCombat(Unit* /*who*/) override
			{
				if (!UpdateVictim())
					return;

				Mana = me->GetPower(POWER_MANA);
				Unit* victim = me->GetVictim();
				Dist = me->GetDistance(victim);
				me->SetReactState(REACT_AGGRESSIVE);

				// Forcer le choix de la Sp�cialisation par creature_template->pickpocketloot
				ForceBranche = me->GetCreatureTemplate()->pickpocketLootId;							// creature_template->pickpocketloot
				if (ForceBranche == 1) { BrancheSpe = 1; }											// branche1 forc�
				else if (ForceBranche == 2) { BrancheSpe = 2; }										// branche2 forc�
				else if (ForceBranche == 3) { BrancheSpe = 3; }										// branche3 forc� 
				else
				{
					// Sinon Choix de la Sp�cialisation Al�atoire
					BrancheSpe = urand(1, NbrDeSpe);
				}

				if ((BrancheSpe > NbrDeSpe) || (BrancheSpe == 0)) { BrancheSpe = 1; }	

				switch(BrancheSpe)
					{
					case 1: // Si Sp�cialisation Vindice -------------------------------------------------------------------------------------------------------
						me->CastSpell(me, Buf_branche1, true);
						me->CastSpell(me, Buf_branche1a, true);
						me->LoadEquipment(1, true);													// creature_equip_template 1

						Spell_branche1_agro = branche1_agro[urand(0,3)];
						Spell_branche1_1 = branche1_1[urand(0, 1)];
						Spell_branche1_2 = branche1_2[urand(0, 1)];
						Spell_branche1_3 = branche1_3[urand(0, 2)];

						Random = urand(1, 2); 
						if (Random == 1 && UpdateVictim()) { DoCastVictim(Spell_branche1_agro); }	// 1/2 Chance de lancer le sort d'agro

						Bonus_Armure(150);															// Bonus d'armure +50%
					break;

					case 2: // Si Sp�cialisation Sacr�  --------------------------------------------------------------------------------------------------------
						me->CastSpell(me, Buf_branche2, true);										// Buf2 sur lui meme
						me->CastSpell(me, Buf_branche2a, true);
						me->LoadEquipment(2, true);													// creature_equip_template 2

						Spell_branche2_agro = branche2_agro[urand(0, 3)];
						Spell_branche2_1 = branche2_1[urand(0, 1)];
						Spell_branche2_2 = branche2_2[urand(0, 1)];
						Spell_branche2_3 = branche2_3[urand(0, 1)];

						Random = urand(1, 2);
						if (Random == 1 && UpdateVictim()) { DoCastVictim(Spell_branche2_agro); }	// 1/2 Chance de lancer le sort d'agro

						Bonus_Armure(125);															// Bonus d'armure +25%
					break;

					case 3: // Si Sp�cialisation Protection ----------------------------------------------------------------------------------------------------
						me->CastSpell(me, Buf_branche3, true);										// Buf3 sur lui meme
						me->CastSpell(me, Buf_branche3a, true);
						me->LoadEquipment(3, true);													// creature_equip_template 3

						Spell_branche3_agro = branche3_agro[urand(0, 2)];
						Spell_branche3_1 = branche3_1[urand(0, 1)];
						Spell_branche3_2 = branche3_2[urand(0, 1)];
						Spell_branche3_3 = branche3_3[urand(0, 1)];

						Random = urand(1, 2);
						if (Random == 1 && UpdateVictim()) { DoCastVictim(Spell_branche3_agro); }	// 1/2 Chance de lancer le sort d'agro

						Bonus_Armure(200);															// Bonus d'armure +100%
						break;
						
					}
			}
			void EnterEvadeMode(EvadeReason /*why*/) override
			{
				RetireBugDeCombat();
				me->AddUnitState(UNIT_STATE_EVADE);
				me->SetSpeedRate(MOVE_RUN, 1.5f);										// Vitesse de d�placement
				me->GetMotionMaster()->MoveTargetedHome();								// Retour home
			}
			void JustReachedHome() override
			{
				me->RemoveAura(Buf_branche1);
				me->RemoveAura(Buf_branche1a);
				me->RemoveAura(Buf_branche2);
				me->RemoveAura(Buf_branche2a);
				me->RemoveAura(Buf_branche3);
				me->RemoveAura(Buf_branche3a);

				Bonus_Armure(100);													// Retire bonus d'armure

				me->SetReactState(REACT_AGGRESSIVE);
				me->SetSpeedRate(MOVE_RUN, 1.01f);									// Vitesse par defaut d�finit a 1.01f puisque le patch modification par type,famille test si 1.0f
			}
			void UpdateAI(uint32 diff) override
			{
				// Emotes hors combat & mouvement -----------------------------------------------------------------------------------------------------------------
				if ((Cooldown_Npc_Emotes <= diff) && (!me->isMoving()) && (!me->IsInCombat()))
				{
				uint32 Npc_Play_Emotes = Npc_Emotes[urand(0, 21)];
				me->HandleEmoteCommand(Npc_Play_Emotes);
				Cooldown_Npc_Emotes = urand(8000, 15000);
				} else Cooldown_Npc_Emotes -= diff;

				// En Combat --------------------------------------------------------------------------------------------------------------------------------------
				if (!UpdateVictim() /*|| me->isPossessed() || me->IsCharmed() || me->HasAuraType(SPELL_AURA_MOD_FEAR)*/)
					return;

				Mana = me->GetPower(POWER_MANA);
				Unit* victim = me->GetVictim();
				Dist = me->GetDistance(victim);

				// Combat suivant la Sp�cialisation
				switch (BrancheSpe)
				{
				case 1: // Sp�cialisation Vindice #######################################################################################################################

					Heal_En_Combat_Melee(diff);
					Combat_Vindice(diff);
					Mouvement_All();
					Mouvement_Contact(diff);
					break;

				case 2: // Sp�cialisation Sacr� #########################################################################################################################

					Heal_En_Combat_Heal(diff);
					Combat_Sacre(diff);
					Mouvement_All();
					Mouvement_Contact(diff);
					break;

				case 3: // Sp�cialisation Protection ####################################################################################################################

					Heal_En_Combat_Melee(diff);
					Combat_Protection(diff);
					Mouvement_All();
					Mouvement_Contact(diff);
					break;

				}
			}

			void RetireBugDeCombat()
			{
				me->CombatStop(true);
				me->DeleteThreatList();
				me->LoadCreaturesAddon();
				me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE);				// UNROOT
				me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);					// Retire flag "en combat"
				me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);				// Retire flag "non attaquable"
				}

			void Mouvement_All()
			{
				if (!UpdateVictim())
					return;

				Dist = me->GetDistance(me->GetVictim());
				if (me->GetDistance2d(me->GetHomePosition().GetPositionX(), me->GetHomePosition().GetPositionY()) > 40)
				{
					RetireBugDeCombat();
					me->AddUnitState(UNIT_STATE_EVADE);
					EnterEvadeMode(EVADE_REASON_SEQUENCE_BREAK);						// Quite le combat si la cible > 30m (Caster & M�l�e) ou > 40m de home
				}
			}
			void Mouvement_Contact(uint32 diff)
			{
				if (!UpdateVictim())
					return;

				Mana = me->GetPower(POWER_MANA);
				Unit* victim = me->GetVictim();
				Dist = me->GetDistance(victim);
				DoMeleeAttackIfReady();												// Combat en m�l�e
				if (!me->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT)) { me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT); }

				// Si la cible >= 6m (pour �viter bug de rester fig�) ------------------------------------------------------------------------------------------
				if (Dist >= 6 && Cooldown_Anti_Bug_Figer <= diff)
				{
					float x, y, z;
					x = victim->GetPositionX();
					y = victim->GetPositionY();
					z = victim->GetPositionZ();
					me->GetClosePoint(x, y, z, victim->GetObjectSize() / 3, 3);
					me->GetMotionMaster()->MovePoint(0xFFFFFE, x, y, z);
					Cooldown_Anti_Bug_Figer = 2000;
				}
				else Cooldown_Anti_Bug_Figer -= diff;

				// Si la cible < 6m ----------------------------------------------------------------------------------------------------------------------------

				if (Dist < 6 && (Cooldown_ResteADistance <= diff) && BrancheSpe != 3)
				{
					Random = urand(1, 5);
					if (Random == 1)
					{
						Tourne_Au_Tour_En_Combat();													// 1 chances sur 5 tourne au tour de sa victime
					}
					else if (Random == 2)
					{
						Avance_3m_En_Combat();														// 1 chances sur 5 avance
					}
					Cooldown_ResteADistance = urand(5000, 7000);
				}
				else Cooldown_ResteADistance -= diff;

			}
			void Tourne_Au_Tour_En_Combat()
			{
				if (!UpdateVictim())
					return;
				Unit* victim = me->GetVictim();

				float x, y, z;
				x = (victim->GetPositionX() + urand(0, 4) - 2);
				y = (victim->GetPositionY() + urand(0, 4) - 2);
				z = victim->GetPositionZ();
				me->GetMotionMaster()->MovePoint(0, x, y, z);

			}
			void Avance_3m_En_Combat()
			{
				if (!UpdateVictim())
					return;

				float x, y, z;
				me->GetClosePoint(x, y, z, me->GetObjectSize() / 3, 3);
				me->GetMotionMaster()->MovePoint(0, x, y, z);
			}

			void Combat_Vindice(uint32 diff)
			{
				if (!UpdateVictim())
					return;

				Mana = me->GetPower(POWER_MANA);
				Unit* victim = me->GetVictim();
				Dist = me->GetDistance(victim);

					// Regen mana en combat ------------------------------------------------------------------------------------------------------------------------
					if (Cooldown_RegenMana <= diff)
					{
						me->SetPower(POWER_MANA, Mana + (MaxMana / 3));
						if (Mana > MaxMana) { me->SetPower(POWER_MANA, MaxMana); }
						Cooldown_RegenMana = 1000;
					}
					else Cooldown_RegenMana -= diff;

					// Combat --------------------------------------------------------------------------------------------------------------------------------------
					Bonus_Degat_Arme_Done(50);										// Bonus de d�gat 

					// Spell1 sur la cible
					if (Cooldown_Spell1 <= diff)
					{
						DoCastVictim(Spell_branche1_1);
						Cooldown_Spell1 = 4000;
					}
					else Cooldown_Spell1 -= diff;

					// Spell2 sur la cible
					if (Cooldown_Spell2 <= diff)
					{
						DoCastVictim(Spell_branche1_2);
						Cooldown_Spell2 = 2500;
					}
					else Cooldown_Spell2 -= diff;

					// Spell3 sur la cible 
					if (Cooldown_Spell3 <= diff)
					{
						DoCastVictim(Spell_branche1_3);
						Cooldown_Spell3 = 4000;
					}
					else Cooldown_Spell3 -= diff;

					Bonus_Degat_Arme_Done(-50);										// Retrait Bonus de d�gat 
			}
			void Combat_Sacre(uint32 diff)
			{
				if (!UpdateVictim())
					return;

				Mana = me->GetPower(POWER_MANA);
				Unit* victim = me->GetVictim();
				Dist = me->GetDistance(victim);

				// Regen mana en combat ------------------------------------------------------------------------------------------------------------------------
				if (Cooldown_RegenMana <= diff)
				{
					me->SetPower(POWER_MANA, Mana + (MaxMana / 2));
					if (Mana > MaxMana) { me->SetPower(POWER_MANA, MaxMana); }
					Cooldown_RegenMana = 1000;
				}
				else Cooldown_RegenMana -= diff;

				Bonus_Degat_Arme_Done(-50);											// Reduction des degats parce que trop trop puissant

				// Combat --------------------------------------------------------------------------------------------------------------------------------------
					// Spell1 sur la cible
					if (Cooldown_Spell1 <= diff)
					{
						DoCastVictim(Spell_branche2_1);
						Cooldown_Spell1 = 4000;
					}
					else Cooldown_Spell1 -= diff;

					// Spell2 sur la cible
					if (Cooldown_Spell2 <= diff)
					{
						DoCastVictim(Spell_branche2_2);
						Cooldown_Spell2 = 2500;
					}
					else Cooldown_Spell2 -= diff;

					// Spell3 sur la cible
					if (Cooldown_Spell3 <= diff)
					{
						DoCastVictim(Spell_branche2_3);
						Cooldown_Spell3 = urand(5000, 7000);
					}
					else Cooldown_Spell3 -= diff;

					Bonus_Degat_Arme_Done(50);										// degats normaux
				}
			void Combat_Protection(uint32 diff)
			{
				if (!UpdateVictim())
					return;

				Mana = me->GetPower(POWER_MANA);
				Unit* victim = me->GetVictim();
				Dist = me->GetDistance(victim);

				// Regen mana en combat ------------------------------------------------------------------------------------------------------------------------
				if (Cooldown_RegenMana <= diff)
				{
					me->SetPower(POWER_MANA, Mana + (MaxMana / 2));
					if (Mana > MaxMana) { me->SetPower(POWER_MANA, MaxMana); }
					Cooldown_RegenMana = 1000;
				}
				else Cooldown_RegenMana -= diff;
				
				// Combat ------------------------------------------------------------------------------------------------------------------------------------------

				Bonus_Degat_Arme_Done(-40);											// Reduction des degats inflig�s -40%
					// Spell1 sur la cible chaque (Sort R�guli�)
					if (Cooldown_Spell1 <= diff)
					{
						me->CastSpell(victim, Spell_branche3_1, true); 
						Cooldown_Spell1 = 4000;
						DoMeleeAttackIfReady();						// Combat en m�l�e
					}
					else Cooldown_Spell1 -= diff;

					// Spell2 sur la cible chaque (Sort secondaire plus lent)
					if (Cooldown_Spell2 <= diff)
					{
						me->CastSpell(victim, Spell_branche3_2, true);
						Cooldown_Spell2 = 4000;
					}
					else Cooldown_Spell2 -= diff;

					// Spell3 sur la cible  (Sort secondaire tres lent , g�n�ralement utilis� comme Dot)
					if (Cooldown_Spell3 <= diff)
					{
						me->CastSpell(victim, Spell_branche3_3, true);
						Cooldown_Spell3 = urand(10000, 12000);
					}
					else Cooldown_Spell3 -= diff;

					Bonus_Degat_Arme_Done(40);										// Reduction des degats inflig�s normaux


			}

			void Heal_En_Combat_Heal(uint32 diff)
			{
				// Heal en combat ------------------------------------------------------------------------------------------------------------------------------
				if (Cooldown_Spell_Heal <= diff)
				{
					Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 1, DistanceDeCast, true);		// pour heal friend

																									// heal sur lui meme
					if ((me->GetHealth() < (me->GetMaxHealth()*0.50)))								// Si PV < 50%
					{
						DoCast(me, Spell_Heal_Heal);
						Cooldown_Spell_Heal = 5000;
					}

					// heal sur Friend 
					if (target = DoSelectLowestHpFriendly(DistanceDeCast))							// Distance de l'alli� = 30m
					{
						if (me->IsFriendlyTo(target) && (me != target))
						{
							if (target->GetHealth() < (target->GetMaxHealth()*0.50))				// Si PV < 50%
							{
								DoCast(target, Spell_Heal_Heal);
								Cooldown_Spell_Heal = 5000;
							}
						}
					}
				}
				else Cooldown_Spell_Heal -= diff;
			}
			void Heal_En_Combat_Melee(uint32 diff)
			{
				// Heal en combat ------------------------------------------------------------------------------------------------------------------------------
				if (Cooldown_Spell_Heal <= diff)
				{
					// heal sur lui meme
					if ((me->GetHealth() < (me->GetMaxHealth()*0.50)))								// Si PV < 50%
					{
						DoCast(me, Spell_Heal_Caster);
						Cooldown_Spell_Heal = 6000;
					}
				}
				else Cooldown_Spell_Heal -= diff;
			}


			void Bonus_Degat_Arme_Done(int val) // 
			{
				// +- Bonus en % de degat des armes inflig�es a victim
				me->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, val, true);
				me->HandleStatModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT, val, true);
				me->HandleStatModifier(UNIT_MOD_DAMAGE_RANGED, TOTAL_PCT, val, true);
			}
			void Bonus_Armure(int val) // 
			{
				// +- Bonus d'armure 100% = pas de bonus/malus   ex : Bonus_Armure(115); // Bonus d'armure +15%
				me->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, me->GetArmor() * (val / 100));
				me->SetCanModifyStats(true);
				me->UpdateAllStats();
			}
		};



		CreatureAI* GetAI(Creature* creature) const override
		{
			return new Stitch_npc_ai_paladinAI(creature);
		}
};

//##################################################################################################################################################################
void AddSC_npc_ai_paladin()
{
	new Stitch_npc_ai_paladin();
}















