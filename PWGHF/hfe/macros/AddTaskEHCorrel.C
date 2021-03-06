AliAnalysisTask *AddTaskEHCorrel(TString ContNameExt = "", Bool_t isPbPb=kFALSE, Bool_t ispp=kTRUE,
    Double_t centMin=0, Double_t centMax=20,
    Bool_t EleSPDkFirst=kFALSE, Bool_t trigElePtcut=kTRUE,Bool_t MEBinChange=kFALSE,
    Int_t MinNClsPE=70, Double_t PtPE=0.1, Double_t invmasscut=0.14,
    Int_t MinNCrossRHad=60,Double_t MinRatioNCrossRHad=0.6, Bool_t HadSPDkAny=kFALSE, Bool_t HadLargITSNCls=kFALSE,
    Bool_t HadFiducialCut = kFALSE, Bool_t HadPosEtaOnly=kFALSE, Bool_t HadNegEtaOnly = kFALSE,
    Int_t MinTPCNCrossRE=70,Double_t MinRatioTPCNCrossRE=0.8, Int_t MinITSNClsE=2, Double_t nsigMin=-1, Double_t nsigMax=3,
    Double_t m02Min=0.02,  Double_t m02Max=0.9, Double_t eovpMin=0.8, Double_t eovpMax=1.2,
    Bool_t useTender = kTRUE, Bool_t EMCtimeCut = kFALSE,
    Bool_t ClsTypeEMC=kTRUE, Bool_t ClsTypeDCAL=kTRUE,
    Int_t PhysSel = AliVEvent::kINT7, Int_t AddPileUpCut=kFALSE, Int_t hadCutCase=2,
    Bool_t applyEleEffi=kFALSE)
{
  //get the current analysis manager
  AliAnalysisManager *mgr = AliAnalysisManager::GetAnalysisManager();
  if (!mgr) {
    Error("AddTaskHFEElecHadronCorrlPbPb", "No analysis manager found.");
    return 0;
  }

  if (!mgr->GetInputEventHandler()) {
    ::Error("AddTaskHFEElecHadronCorrlPbPb", "This task requires an input event handler");
    return NULL;
  }
  TString type = mgr->GetInputEventHandler()->GetDataType(); // can be "ESD" or "AOD"
  /*  if (type=="AOD"){
      ::Error("AddTaskHFEElecHadronCorrlPbPb", "The tasks exits because AODs are in input");
      return NULL;
      }
   */
  Bool_t MCthere=kTRUE;
  AliMCEventHandler *mcH = dynamic_cast<AliMCEventHandler*>(mgr->GetMCtruthEventHandler());
  if(!mcH){
    MCthere=kFALSE;
  }

  if(ClsTypeEMC && !ClsTypeDCAL)ContNameExt+="_EMC";
  if(!ClsTypeEMC && ClsTypeDCAL)ContNameExt+="_DCAL";

  if(PhysSel == AliVEvent::kINT7){
    AliAnalysisTaskEHCorrel *taskHFEeh = new AliAnalysisTaskEHCorrel("eh");
    taskHFEeh->SelectCollisionCandidates(AliVEvent::kINT7);
    taskHFEeh->IsPbPb(isPbPb);
    taskHFEeh->Ispp(ispp);
    taskHFEeh->SetCentralitySelection(centMin,centMax);
    taskHFEeh->SetMinTPCNCrossRElec(MinTPCNCrossRE);
    taskHFEeh->SetMinRatioTPCNCrossRElec(MinRatioTPCNCrossRE);
    taskHFEeh->SetMinITSNClsElec(MinITSNClsE);
    taskHFEeh->SetTPCnsigCut(nsigMin,nsigMax);
    taskHFEeh->SetM02Cut(m02Min,m02Max);
    taskHFEeh->SetEovPCut(eovpMin,eovpMax);
    taskHFEeh->SetHadronCutCase(hadCutCase);
    taskHFEeh->SetTriggerElePtCut(trigElePtcut);
    taskHFEeh->SetClusterTypeEMC(ClsTypeEMC);
    taskHFEeh->SetClusterTypeDCAL(ClsTypeDCAL);
    taskHFEeh->SetPartnerEleMinTPCNCls(MinNClsPE);
    taskHFEeh->SetPartnerEleMinPt(PtPE);
    taskHFEeh->SetInvmassCut(invmasscut);
    taskHFEeh->SetHadMinTPCNCrossR(MinNCrossRHad);
    taskHFEeh->SetHadMinRatioTPCNCrossR(MinRatioNCrossRHad);
    taskHFEeh->SetHadSPDkAny(HadSPDkAny);
    taskHFEeh->SetHadLargeITSNCls(HadLargITSNCls);
    taskHFEeh->SetHadFiducialCut(HadFiducialCut);
    taskHFEeh->SetHadPosEtaOnly(HadPosEtaOnly);
    taskHFEeh->SetHadNegEtaOnly(HadNegEtaOnly);
    taskHFEeh->SetMEBinChange(MEBinChange);
    taskHFEeh->SetElecSPDkFirst(EleSPDkFirst);
    taskHFEeh->SetTenderSwitch(useTender);
    taskHFEeh->SetEMCClsTimeCut(EMCtimeCut);
    taskHFEeh->SetAdditionalPileUpCuts(AddPileUpCut);
    taskHFEeh->SetElecEffi(applyEleEffi);

    TString containerName = mgr->GetCommonFileName();
    TString SubcontainerName = ContNameExt;
    SubcontainerName += "_EHPbPb_INT7";
    AliAnalysisDataContainer *coutput3 = mgr->CreateContainer(SubcontainerName,TList::Class(),AliAnalysisManager::kOutputContainer,containerName.Data());

    mgr->ConnectInput(taskHFEeh,0,mgr->GetCommonInputContainer());
    mgr->ConnectOutput(taskHFEeh,1,coutput3);
    //mgr->AddTask(taskHFEeh);
  }

  if(PhysSel == AliVEvent::kEMCEGA){
    // EMCal EGA EG1
    AliAnalysisTaskEHCorrel *taskHFEehGA01 = new AliAnalysisTaskEHCorrel("ehGA");
    taskHFEehGA01->SelectCollisionCandidates(AliVEvent::kEMCEGA);
    taskHFEehGA01->IsPbPb(isPbPb);
    taskHFEehGA01->Ispp(ispp);
    taskHFEehGA01->SetEMCalTriggerEG1(kTRUE);
    taskHFEehGA01->SetCentralitySelection(centMin,centMax);
    taskHFEehGA01->SetHadronCutCase(hadCutCase);
    taskHFEehGA01->SetTriggerElePtCut(trigElePtcut);
    taskHFEehGA01->SetClusterTypeEMC(ClsTypeEMC);
    taskHFEehGA01->SetClusterTypeDCAL(ClsTypeDCAL);

    TString containerName01 = mgr->GetCommonFileName();
    TString SubcontainerName01 = ContNameExt;
    SubcontainerName01 += "_EH_PbPb_GA1";
    AliAnalysisDataContainer *cinput  = mgr->GetCommonInputContainer();
    AliAnalysisDataContainer *coutput1 = mgr->CreateContainer(SubcontainerName01, TList::Class(),AliAnalysisManager::kOutputContainer, containerName01.Data());

    mgr->ConnectInput(taskHFEehGA01, 0, cinput);
    mgr->ConnectOutput(taskHFEehGA01, 1, coutput1);
    //mgr->AddTask(taskHFEehGA01);
  }
  return taskHFEeh;
}
