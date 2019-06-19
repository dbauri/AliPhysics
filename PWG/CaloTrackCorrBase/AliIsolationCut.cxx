/**************************************************************************
 * Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
 *                                                                        *
 * Author: The ALICE Off-line Project.                                    *
 * Contributors are mentioned in the code where appropriate.              *
 *                                                                        *
 * Permission to use, copy, modify and distribute this software and its   *
 * documentation strictly for non-commercial purposes is hereby granted   *
 * without fee, provided that the above copyright notice appears in all   *
 * copies and that both the copyright notice and this permission notice   *
 * appear in the supporting documentation. The authors make no claims     *
 * about the suitability of this software for any purpose. It is          *
 * provided "as is" without express or implied warranty.                  *
 **************************************************************************/

// --- ROOT system ---
#include <TObjArray.h>

// --- AliRoot system ---
#include "AliCaloTrackParticleCorrelation.h"
#include "AliEMCALGeometry.h"
#include "AliEMCALGeoParams.h"
#include "AliAODTrack.h"
#include "AliVCluster.h"
#include "AliMixedEvent.h"
#include "AliLog.h"

// --- CaloTrackCorrelations --- 
#include "AliCaloTrackReader.h"
#include "AliCalorimeterUtils.h"
#include "AliCaloPID.h"
#include "AliFiducialCut.h"
#include "AliHistogramRanges.h"

#include "AliIsolationCut.h"

/// \cond CLASSIMP
ClassImp(AliIsolationCut) ;
/// \endcond

//____________________________________
/// Default constructor. Initialize parameters
//____________________________________
AliIsolationCut::AliIsolationCut() :
TObject(),
fFillHistograms(0),
fConeSize(0.),       fPtThreshold(0.),              fPtThresholdMax(10000.),
fSumPtThreshold(0.), fSumPtThresholdMax(10000.),    fPtFraction(0.),     
fICMethod(0),        fPartInCone(0),
fFracIsThresh(1),    fIsTMClusterInConeRejected(1), fDistMinToTrigger(-1.),
fDebug(0),           fMomentum(),                   fTrackVector(),
// Histograms
fHistoRanges(0),
fhPtInCone(0),       fhPtClusterInCone(0),          fhPtTrackInCone(0),
fhConeSumPt(0),      fhConeSumPtCluster(0),         fhConeSumPtTrack(0),
fhConePtLead(0),     fhConePtLeadCluster(0),        fhConePtLeadTrack(0),
fhConeSumPtClustervsTrack(0),     fhConeSumPtClusterTrackFrac(0),
fhConePtLeadClustervsTrack(0),    fhConePtLeadClusterTrackFrac(0),
fhConeSumPtTrigEtaPhi(0)
{
  InitParameters();
}

//_________________________________________________________________________________________________________________________________
/// Get the pt sum of the clusters inside the cone, the leading cluster pT and number of clusters 
///
/// \param aodParticle: Kinematics and + of candidate particle for isolation.
/// \param reader: pointer to AliCaloTrackReader. Needed to access event info.
/// \param bFillAOD: Indicate if particles in cone must be added to AOD particle object.
/// \param useRefs: Get the list of tracks or clusters in cone from references
/// \param aodArrayRefName: Name of array where list of tracks/clusters in cone is stored.
/// \param bgCls: List of clusters from mixed event background pool AliAnaParticleHadronCorrelation.
/// \param calorimeter: Which input trigger calorimeter used
/// \param pid: pointer to AliCaloPID. Needed to reject matched clusters in isolation cone.
/// \param nPart: number of tracks/clusters above threshold in cone, output.
/// \param nfrac: 1 if fraction pT cluster-track / pT trigger in cone avobe threshold, output.
/// \param coneptsumCluster: total momentum energy in cone (track+cluster), output.
/// \param coneptLeadCluster: momentum of leading cluster or track in cone, output.
/// \param histoWeight: Histograms weight (event, pt depedent)
//_________________________________________________________________________________________________________________________________
void AliIsolationCut::CalculateCaloSignalInCone 
(
 AliCaloTrackParticleCorrelation * pCandidate, AliCaloTrackReader * reader,
 Bool_t    bFillAOD           , Bool_t    useRefs, 
 TString   aodArrayRefName    , TObjArray  * bgCls,
 Int_t     calorimeter        , AliCaloPID * pid,
 Int_t   & nPart              , Int_t   & nfrac,
 Float_t & coneptsumCluster   , Float_t & coneptLeadCluster,
 Float_t & etaBandPtSumCluster, Float_t & phiBandPtSumCluster, 
 Double_t histoWeight
) 
{
  if ( fPartInCone == kOnlyCharged ) return ;
  
  // Get the array with clusters
  //
  TObjArray * plNe = 0x0; 
  
  if ( bgCls )
  {
    plNe = bgCls ;
  }
  else if ( !useRefs )
  {
    if      (calorimeter == AliFiducialCut::kPHOS )
      plNe = reader->GetPHOSClusters();
    else if (calorimeter == AliFiducialCut::kEMCAL)
      plNe = reader->GetEMCALClusters();
  }
  else
  {
    plNe = pCandidate->GetObjArray(aodArrayRefName+"Clusters"); 
  }
  
  if ( !plNe ) 
  {
    fhConeSumPtCluster ->Fill(pCandidate->Pt(), 0., histoWeight);
    
    fhConePtLeadCluster->Fill(pCandidate->Pt(), 0., histoWeight);
  
    if ( coneptLeadCluster > 0  || coneptsumCluster > 0 ) 
      AliError(Form("No ref clusters!!! sum %f, lead %f",coneptsumCluster,coneptLeadCluster));
    
    return ;
  }
  
  // Init parameters
  //
  Float_t ptC   = pCandidate->Pt() ;
  Float_t phiC  = pCandidate->Phi() ;
  if ( phiC < 0 ) phiC+=TMath::TwoPi();
  Float_t etaC  = pCandidate->Eta() ;
  
  Float_t pt     = -100. ;
  Float_t eta    = -100. ;
  Float_t phi    = -100. ;
  Float_t rad    = -100. ;
  
  TObjArray * refclusters  = 0x0;
  Int_t       nclusterrefs = 0;
  
  // Get the clusters
  //
  for(Int_t ipr = 0;ipr < plNe->GetEntries() ; ipr ++ )
  {
    AliVCluster * calo = dynamic_cast<AliVCluster *>(plNe->At(ipr)) ;
    
    if(calo)
    {
      // Get the index where the cluster comes, to retrieve the corresponding vertex
      Int_t evtIndex = 0 ;
      if (reader->GetMixedEvent())
        evtIndex=reader->GetMixedEvent()->EventIndexForCaloCluster(calo->GetID()) ;
      
      
      // Do not count the candidate (photon or pi0) or the daughters of the candidate
      if(calo->GetID() == pCandidate->GetCaloLabel(0) ||
         calo->GetID() == pCandidate->GetCaloLabel(1)   ) continue ;
      
      // Skip matched clusters with tracks in case of neutral+charged analysis
      if(fIsTMClusterInConeRejected)
      {
        if( fPartInCone == kNeutralAndCharged &&
           pid->IsTrackMatched(calo,reader->GetCaloUtils(),reader->GetInputEvent()) ) continue ;
      }
      
      // Assume that come from vertex in straight line
      calo->GetMomentum(fMomentum,reader->GetVertex(evtIndex)) ;
      
      pt  = fMomentum.Pt()  ;
      eta = fMomentum.Eta() ;
      phi = fMomentum.Phi() ;
    }
    else
    {// Mixed event stored in AliCaloTrackParticles
      AliCaloTrackParticle * calomix = dynamic_cast<AliCaloTrackParticle*>(plNe->At(ipr)) ;
      if(!calomix)
      {
        AliWarning("Wrong calo data type, continue");
        continue;
      }
      
      pt  = calomix->Pt();
      eta = calomix->Eta();
      phi = calomix->Phi() ;
    }
    
    // ** Calculate distance between candidate and tracks **
    
    if( phi < 0 ) phi+=TMath::TwoPi();
    
    rad = Radius(etaC, phiC, eta, phi);
    
    // ** Exclude clusters too close to the candidate, inactive by default **
    
    if(rad < fDistMinToTrigger) continue ;
    
    // ** For the background out of cone **
    
    if(rad > fConeSize)
    {
      if(eta > (etaC-fConeSize) && eta < (etaC+fConeSize)) phiBandPtSumCluster += pt;
      if(phi > (phiC-fConeSize) && phi < (phiC+fConeSize)) etaBandPtSumCluster += pt;
    }
    
    // ** For the isolated particle **
    
    // Only loop the particle at the same side of candidate
    if(TMath::Abs(phi-phiC)>TMath::PiOver2()) continue ;
    
    //      // If at the same side has particle larger than candidate,
    //      // then candidate can not be the leading, skip such events
    //      if(pt > ptC)
    //      {
    //        n         = -1;
    //        nfrac     = -1;
    //        coneptsumCluster = -1;
    //        isolated  = kFALSE;
    //
    //        pCandidate->SetLeadingParticle(kFALSE);
    //
    //        if(bFillAOD)
    //        {
    //          if(reftracks)
    //          {
    //            reftracks  ->Clear();
    //            delete reftracks;
    //          }
    //
    //          if(refclusters)
    //          {
    //            refclusters->Clear();
    //            delete refclusters;
    //          }
    //        }
    //        return ;
    //      }
    
    AliDebug(2,Form("\t Cluster %d, pT %2.2f, eta %1.2f, phi %2.2f, R candidate %2.2f", 
                    ipr,pt,eta,phi,rad));
    
    //
    // Select calorimeter clusters inside the isolation radius 
    //
    if ( rad < fConeSize )
    {
      AliDebug(2,"Inside candidate cone");
      
      if ( bFillAOD )
      {
        nclusterrefs++;
        
        if ( nclusterrefs == 1 )
        {
          refclusters = new TObjArray(0);
          //refclusters->SetName(Form("Clusters%s",aodArrayRefName.Data()));
          TString tempo(aodArrayRefName)  ;
          tempo += "Clusters" ;
          refclusters->SetName(tempo);
          refclusters->SetOwner(kFALSE);
        }
        
        refclusters->Add(calo);
      }
      
      coneptsumCluster+=pt;
      
      if ( coneptLeadCluster < pt ) coneptLeadCluster = pt;
      
      if ( fFillHistograms )
      {
        fhPtInCone       ->Fill(ptC, pt, histoWeight);
        fhPtClusterInCone->Fill(ptC, pt, histoWeight);
      }
      
      // *Before*, count particles in cone
      //
      if ( pt > fPtThreshold && pt < fPtThresholdMax )  nPart++;
      
      //if fPtFraction*ptC<fPtThreshold then consider the fPtThreshold directly
      if ( fFracIsThresh )
      {
        if ( fPtFraction*ptC < fPtThreshold )
        {
          if ( pt > fPtThreshold )    nfrac++ ;
        }
        else
        {
          if ( pt > fPtFraction*ptC ) nfrac++;
        }
      }
      else
      {
        if ( pt > fPtFraction*ptC ) nfrac++;
      }
      
    }//in cone
    
  }// neutral particle loop
  
  if ( fFillHistograms )
  {
    fhConeSumPtCluster ->Fill(ptC, coneptsumCluster , histoWeight);
    fhConePtLeadCluster->Fill(ptC, coneptLeadCluster, histoWeight);
  }
  
  // Add reference arrays to AOD when filling AODs only
  if ( bFillAOD && refclusters ) pCandidate->AddObjArray(refclusters);  
}

//_________________________________________________________________________________________________________________________________
/// Get the pt sum of the tracks inside the cone, the leading track pT and number of clusters 
///
/// \param aodParticle: Kinematics and + of candidate particle for isolation.
/// \param reader: pointer to AliCaloTrackReader. Needed to access event info.
/// \param bFillAOD: Indicate if particles in cone must be added to AOD particle object.
/// \param useRefs: Get the list of tracks or clusters in cone from references
/// \param aodArrayRefName: Name of array where list of tracks/clusters in cone is stored.
/// \param nPart: number of tracks/clusters above threshold in cone, output.
/// \param nfrac: 1 if fraction pT cluster-track / pT trigger in cone avobe threshold, output.
/// \param coneptsumCluster: total momentum energy in cone (track+cluster), output.
/// \param coneptLeadCluster: momentum of leading cluster or track in cone, output.
/// \param histoWeight: Histograms weight (event, pt depedent)
//_________________________________________________________________________________________________________________________________
void AliIsolationCut::CalculateTrackSignalInCone
(
 AliCaloTrackParticleCorrelation * pCandidate, AliCaloTrackReader * reader,
 Bool_t    bFillAOD         , Bool_t    useRefs, 
 TString   aodArrayRefName  , TObjArray * bgTrk,
 Int_t   & nPart            , Int_t   & nfrac,
 Float_t & coneptsumTrack   , Float_t & coneptLeadTrack,
 Float_t & etaBandPtSumTrack, Float_t & phiBandPtSumTrack,
 Double_t histoWeight
) 
{  
  if ( fPartInCone == kOnlyNeutral ) return ;

  // Get the array with tracks
  //
  TObjArray * plCTS    = 0x0; 
  if ( bgTrk )
  {
    plCTS = bgTrk;
  }
  else if ( !useRefs )
  {
    plCTS = reader->GetCTSTracks();
  }
  else
  {
    plCTS = pCandidate->GetObjArray(aodArrayRefName+"Tracks");
  }
  
  if ( !plCTS ) 
  {
    if(fFillHistograms)
    {
      fhConeSumPtTrack      ->Fill(pCandidate->Pt(), 0., histoWeight);
      
      fhConePtLeadTrack     ->Fill(pCandidate->Pt(), 0., histoWeight);
    } // histograms
    
    if(coneptLeadTrack > 0  || coneptsumTrack > 0) 
      AliError(Form("No ref tracks!!! sum %f, lead %f",coneptsumTrack,coneptLeadTrack));
    
    return ;
  }
  
  //-----------------------------------------------------------
  // Init parameters
  //
  Float_t ptTrig   = pCandidate->Pt() ;
  Float_t phiTrig  = pCandidate->Phi() ;
  if ( phiTrig < 0 ) phiTrig+=TMath::TwoPi();
  Float_t etaTrig  = pCandidate->Eta() ;
 
  Float_t ptTrack  = -100. ;
  Float_t etaTrack = -100. ;
  Float_t phiTrack = -100. ;
  Float_t rad      = -100. ;
  
  TObjArray * reftracks  = 0x0;
  Int_t       ntrackrefs = 0;
    
  //-----------------------------------------------------------
  // Get the tracks in cone
  //
  //-----------------------------------------------------------
  for(Int_t ipr = 0;ipr < plCTS->GetEntries() ; ipr ++ )
  {
    AliVTrack* track = dynamic_cast<AliVTrack*>(plCTS->At(ipr)) ;
    
    if(track)
    {
      // In case of isolation of single tracks or conversion photon (2 tracks) or pi0 (4 tracks),
      // do not count the candidate or the daughters of the candidate
      // in the isolation conte
      if ( pCandidate->GetDetectorTag() == AliFiducialCut::kCTS ) // make sure conversions are tagged as kCTS!!!
      {
        Int_t  trackID   = reader->GetTrackID(track) ; // needed instead of track->GetID() since AOD needs some manipulations
        Bool_t contained = kFALSE;
        
        for(Int_t i = 0; i < 4; i++) 
        {
          if( trackID == pCandidate->GetTrackLabel(i) ) contained = kTRUE;
        }
        
        if ( contained ) continue ;
      }
      
      fTrackVector.SetXYZ(track->Px(),track->Py(),track->Pz());
      ptTrack  = fTrackVector.Pt();
      etaTrack = fTrackVector.Eta();
      phiTrack = fTrackVector.Phi() ;
    }
    else
    {// Mixed event stored in AliCaloTrackParticles
      AliCaloTrackParticle * trackmix = dynamic_cast<AliCaloTrackParticle*>(plCTS->At(ipr)) ;
      if(!trackmix)
      {
        AliWarning("Wrong track data type, continue");
        continue;
      }
      
      ptTrack  = trackmix->Pt();
      etaTrack = trackmix->Eta();
      phiTrack = trackmix->Phi() ;
    }
    
    // ** Calculate distance between candidate and tracks **
    
    if ( phiTrack < 0 ) phiTrack+=TMath::TwoPi();
    
    rad = Radius(etaTrig, phiTrig, etaTrack, phiTrack);
    
    // ** Exclude tracks too close to the candidate, inactive by default **
    
    if ( rad < fDistMinToTrigger ) continue ;
    
    // ** For the background out of cone **
    
    if ( rad > fConeSize )
    {
      if ( etaTrack > (etaTrig-fConeSize) && etaTrack < (etaTrig+fConeSize) ) phiBandPtSumTrack += ptTrack;
      if ( phiTrack > (phiTrig-fConeSize) && phiTrack < (phiTrig+fConeSize) ) etaBandPtSumTrack += ptTrack;
    }
    
    // ** For the isolated particle **
    
    // Only loop the particle at the same side of candidate
    if ( TMath::Abs(phiTrack-phiTrig) > TMath::PiOver2() ) continue ;
    
    //      // If at the same side has particle larger than candidate,
    //      // then candidate can not be the leading, skip such events
    //      if(pt > ptTrig)
    //      {
    //        n         = -1;
    //        nfrac     = -1;
    //        coneptsumTrack = -1;
    //        isolated  = kFALSE;
    //
    //        pCandidate->SetLeadingParticle(kFALSE);
    //
    //        if(bFillAOD && reftracks)
    //        {
    //          reftracks->Clear();
    //          delete reftracks;
    //        }
    //
    //        return ;
    //      }
    
    AliDebug(2,Form("\t Track %d, pT %2.2f, eta %1.2f, phi %2.2f, R candidate %2.2f", 
                    ipr,ptTrack,etaTrack,phiTrack,rad));
    
    //
    // Select tracks inside the isolation radius
    //
    if ( rad > fConeSize ) continue ;
    
    AliDebug(2,"Inside candidate cone");
    
    if ( bFillAOD )
    {
      ntrackrefs++;
      
      if ( ntrackrefs == 1 )
      {
        reftracks = new TObjArray(0);
        //reftracks->SetName(Form("Tracks%s",aodArrayRefName.Data()));
        TString tempo(aodArrayRefName)  ;
        tempo += "Tracks" ;
        reftracks->SetName(tempo);
        reftracks->SetOwner(kFALSE);
      }
      
      reftracks->Add(track);
    }
    
    coneptsumTrack+=ptTrack;
    
    if ( coneptLeadTrack < ptTrack ) coneptLeadTrack = ptTrack;
    
    if ( fFillHistograms )
    {
      fhPtInCone      ->Fill(ptTrig , ptTrack, histoWeight);
      fhPtTrackInCone ->Fill(ptTrig , ptTrack, histoWeight);
    } // histograms
    
    // *Before*, count particles in cone
    if ( ptTrack > fPtThreshold && ptTrack < fPtThresholdMax )  nPart++;
    
    //if fPtFraction*ptTrig<fPtThreshold then consider the fPtThreshold directly
    if ( fFracIsThresh )
    {
      if ( fPtFraction*ptTrig < fPtThreshold )
      {
        if ( ptTrack > fPtThreshold )    nfrac++ ;
      }
      else
      {
        if ( ptTrack > fPtFraction*ptTrig ) nfrac++;
      }
    }
    else
    {
      if ( ptTrack > fPtFraction*ptTrig ) nfrac++;
    }
    
  }// charged particle loop

  if ( fFillHistograms )
  {
    fhConeSumPtTrack ->Fill(ptTrig, coneptsumTrack , histoWeight);
    fhConePtLeadTrack->Fill(ptTrig, coneptLeadTrack, histoWeight);
  }
  
  // Add reference arrays to AOD when filling AODs only
  if ( bFillAOD && reftracks ) pCandidate->AddObjArray(reftracks);  
}

//_________________________________________________________________________________________________________________________________
/// Get normalization of cluster background band.
//_________________________________________________________________________________________________________________________________
void AliIsolationCut::CalculateUEBandClusterNormalization(AliCaloTrackReader * /*reader*/, Float_t   etaC, Float_t /*phiC*/,
                                                           Float_t   phiUEptsumCluster,     Float_t   etaUEptsumCluster,
                                                           Float_t & phiUEptsumClusterNorm, Float_t & etaUEptsumClusterNorm,
                                                           Float_t & excessFracEta,         Float_t & excessFracPhi         ) const
{
  Float_t coneA = fConeSize*fConeSize*TMath::Pi(); // A = pi R^2, isolation cone area
  if ( fDistMinToTrigger > 0 ) coneA -= fDistMinToTrigger*fDistMinToTrigger*TMath::Pi();

  //Careful here if EMCal limits changed .. 2010 (4 SM) to 2011-12 (10 SM), for the moment consider 100 deg in phi
  Float_t emcEtaSize = 0.7*2; // TO FIX
  Float_t emcPhiSize = TMath::DegToRad()*100.; // TO FIX

  /* //Catherine code
   if(((((2*fConeSize*emcPhiSize)-coneA))*phiBandBadCellsCoeff)!=0)phiUEptsumClusterNorm = phiUEptsumCluster*(coneA*coneBadCellsCoeff / (((2*fConeSize*emcPhiSize)-coneA))*phiBandBadCellsCoeff); // pi * R^2 / (2 R * 2 100 deg) -  trigger cone
   if(((((2*(fConeSize-excess)*emcPhiSize)-(coneA-excessFracEta))*etaBandBadCellsCoeff))!=0)phiUEptsumClusterNorm = phiUEptsumCluster*(coneA *coneBadCellsCoeff/ (((2*(fConeSize-excess)*emcPhiSize)-(coneA/excessFracEta))*etaBandBadCellsCoeff));
   if(((2*(fConeSize-excess)*emcEtaSize)-(coneA-excessFracPhi))*phiBandBadCellsCoeff!=0) etaUEptsumClusterNorm = etaUEptsumCluster*(coneA*coneBadCellsCoeff / (((2*(fConeSize-excess)*emcEtaSize)-(coneA/excessFracPhi))*phiBandBadCellsCoeff));
   */

  if((2*fConeSize*emcPhiSize-coneA)!=0) phiUEptsumClusterNorm = phiUEptsumCluster*(coneA / (((2*fConeSize*emcPhiSize)-coneA))); // pi * R^2 / (2 R * 2 100 deg) -  trigger cone
  if((2*fConeSize*emcEtaSize-coneA)!=0) etaUEptsumClusterNorm = etaUEptsumCluster*(coneA / (((2*fConeSize*emcEtaSize)-coneA))); // pi * R^2 / (2 R * 2*0.7)  -  trigger cone

  //out of eta acceptance
  excessFracEta = 1;
  excessFracPhi = 1;

  if(TMath::Abs(etaC)+fConeSize > emcEtaSize/2.)
  {
    Float_t excess = TMath::Abs(etaC) + fConeSize - emcEtaSize/2.;
    excessFracEta  = CalculateExcessAreaFraction(excess);

    if ( excessFracEta != 0) coneA /=  excessFracEta;

    //UE band is also out of acceptance, need to estimate corrected area
    if(((2*fConeSize-excess)*emcPhiSize-coneA) != 0 ) phiUEptsumClusterNorm = phiUEptsumCluster*(coneA / ((((2*fConeSize-excess)*emcPhiSize)-coneA)));
    if(( 2*fConeSize        *emcEtaSize-coneA) != 0 ) etaUEptsumClusterNorm = etaUEptsumCluster*(coneA / ((( 2*fConeSize        *emcEtaSize)-coneA)));
  }
}

//________________________________________________________________________________________________________________________________
/// Get normalization of track background band.
//________________________________________________________________________________________________________________________________
void AliIsolationCut::CalculateUEBandTrackNormalization  (AliCaloTrackReader * reader,    Float_t   etaC, Float_t /*phiC*/,
                                                          Float_t   phiUEptsumTrack,      Float_t   etaUEptsumTrack,
                                                          Float_t & phiUEptsumTrackNorm,  Float_t & etaUEptsumTrackNorm,
                                                          Float_t & excessFracEta,        Float_t & excessFracPhi          ) const
{
  Float_t coneA = fConeSize*fConeSize*TMath::Pi(); // A = pi R^2, isolation cone area
  if ( fDistMinToTrigger > 0 ) coneA -= fDistMinToTrigger*fDistMinToTrigger*TMath::Pi();
  
  // Get the cut used for the TPC tracks in the reader, +-0.8, +-0.9 ...
  // Only valid in simple fidutial cut case and if the cut is applied, careful!
  Float_t tpcEtaSize = reader->GetFiducialCut()->GetCTSFidCutMaxEtaArray()->At(0) -
  reader->GetFiducialCut()->GetCTSFidCutMinEtaArray()->At(0) ;
  Float_t tpcPhiSize = TMath::TwoPi();

  /*//Catherine code
   //phiUEptsumTrackNorm = phiUEptsumTrack*(coneA*coneBadCellsCoeff / (((2*fConeSize*tpcPhiSize)-coneA))*phiBandBadCellsCoeff); // pi * R^2 / (2 R * 2 pi) -  trigger cone
   //etaUEptsumTrackNorm = etaUEptsumTrack*(coneA*coneBadCellsCoeff / (((2*fConeSize*tpcEtaSize)-coneA))*etaBandBadCellsCoeff); // pi * R^2 / (2 R * 1.6)  -  trigger cone
   if((2*fConeSize*tpcPhiSize-coneA)!=0)phiUEptsumTrackNorm = phiUEptsumTrack*(coneA / (((2*fConeSize*tpcPhiSize)-coneA))); // pi * R^2 / (2 R * 2 pi) -  trigger cone
   if((2*fConeSize*tpcEtaSize-coneA)!=0)etaUEptsumTrackNorm = etaUEptsumTrack*(coneA / (((2*fConeSize*tpcEtaSize)-coneA))); // pi * R^2 / (2 R * 1.6)  -  trigger cone
   if((2*(fConeSize-excess)*tpcPhiSize)-(coneA-excessFracEta)!=0)phiUEptsumTrackNorm = phiUEptsumTrack*(coneA / (((2*(fConeSize-excess)*tpcPhiSize)-(coneA/excessFracEta))));
   */ //end Catherine code

  //correct out of eta acceptance
  excessFracEta = 1;
  excessFracPhi = 1;

  if((2*fConeSize*tpcPhiSize-coneA)!=0) phiUEptsumTrackNorm = phiUEptsumTrack*(coneA / (((2*fConeSize*tpcPhiSize)-coneA))); // pi * R^2 / (2 R * 2 pi) -  trigger cone
  if((2*fConeSize*tpcEtaSize-coneA)!=0) etaUEptsumTrackNorm = etaUEptsumTrack*(coneA / (((2*fConeSize*tpcEtaSize)-coneA))); // pi * R^2 / (2 R * 1.6)  -  trigger cone

  if(TMath::Abs(etaC)+fConeSize > tpcEtaSize/2.)
  {
    Float_t excess = TMath::Abs(etaC) + fConeSize - tpcEtaSize/2.;
    excessFracEta  = CalculateExcessAreaFraction(excess);
    if (excessFracEta != 0) coneA /=  excessFracEta;

    //UE band is also out of acceptance, need to estimate corrected area
    if(((2*fConeSize-excess)*tpcPhiSize - coneA) !=0 ) phiUEptsumTrackNorm = phiUEptsumTrack*(coneA / ((((2*fConeSize-excess)*tpcPhiSize)-coneA)));
    if(( 2*fConeSize        *tpcEtaSize - coneA) !=0 ) etaUEptsumTrackNorm = etaUEptsumTrack*(coneA / ((( 2*fConeSize        *tpcEtaSize)-coneA)));
  }
}

//________________________________________________________________________
/// If isolation cone are is outside a detector, calculate the area in excess.
/// \param excess: cone size minus acceptance of detector.
/// \return Area of a circunference segment 1/2 R^2 (angle-sin(angle)), angle = 2*ACos((R-excess)/R).
//________________________________________________________________________
Float_t AliIsolationCut::CalculateExcessAreaFraction(Float_t excess) const
{
  Float_t angle   = 2*TMath::ACos( (fConeSize-excess) / fConeSize );

  Float_t coneA   = fConeSize*fConeSize*TMath::Pi(); // A = pi R^2, isolation cone area

  Float_t excessA = fConeSize*fConeSize / 2 * (angle-TMath::Sin(angle));

  if(coneA > excessA) return coneA / (coneA-excessA);
  else
  {
    AliWarning(Form("Please Check : Excess Track %2.3f, coneA %2.2f,  excessA %2.2f, angle %2.2f,factor %2.2f",
                    excess,coneA, excessA, angle*TMath::RadToDeg(), coneA / (coneA-excessA)));
    return  1;
  }
}

//_________________________________________________________________________________
/// Get good cell density (number of active cells over all cells in cone).
//_________________________________________________________________________________
Float_t AliIsolationCut::GetCellDensity(AliCaloTrackParticleCorrelation * pCandidate,
                                        AliCaloTrackReader * reader) const
{
  Double_t coneCells    = 0.; //number of cells in cone with radius fConeSize
  Double_t coneCellsBad = 0.; //number of bad cells in cone with radius fConeSize
  Double_t cellDensity  = 1.;

  Float_t phiC  = pCandidate->Phi() ;
  if(phiC<0) phiC+=TMath::TwoPi();
  Float_t etaC  = pCandidate->Eta() ;

  if(pCandidate->GetDetectorTag() == AliCaloTrackReader::kEMCAL)
  {
    AliEMCALGeometry* eGeom = AliEMCALGeometry::GetInstance();
    AliCalorimeterUtils *cu = reader->GetCaloUtils();

    Int_t absId = -999;
    if (eGeom->GetAbsCellIdFromEtaPhi(etaC,phiC,absId))
    {
      // Get absolute (col,row) of candidate
      Int_t iEta=-1, iPhi=-1, iRCU = -1;
      Int_t nSupMod = cu->GetModuleNumberCellIndexes(absId, pCandidate->GetDetectorTag(), iEta, iPhi, iRCU);

      Int_t colC = iEta;
      if (nSupMod % 2) colC =  AliEMCALGeoParams::fgkEMCALCols + iEta ;
      Int_t rowC = iPhi + AliEMCALGeoParams::fgkEMCALRows*int(nSupMod/2);

      Int_t sqrSize = int(fConeSize/0.0143) ; // Size of cell in radians
      Int_t status = 0;
      // Loop on cells in a square of side fConeSize to check cells in cone
      for(Int_t icol = colC-sqrSize; icol < colC+sqrSize;icol++)
      {
        for(Int_t irow = rowC-sqrSize; irow < rowC+sqrSize; irow++)
        {
          if (Radius(colC, rowC, icol, irow) < sqrSize)
          {
            coneCells += 1.;

            Int_t cellSM  = -999;
            Int_t cellEta = -999;
            Int_t cellPhi = -999;
            if(icol > AliEMCALGeoParams::fgkEMCALCols-1)
            {
              cellSM = 0+int(irow/AliEMCALGeoParams::fgkEMCALRows)*2;
              cellEta = icol-AliEMCALGeoParams::fgkEMCALCols;
              cellPhi = irow-AliEMCALGeoParams::fgkEMCALRows*int(cellSM/2);
            }
            if(icol < AliEMCALGeoParams::fgkEMCALCols)
            {
              cellSM = 1+int(irow/AliEMCALGeoParams::fgkEMCALRows)*2;
              cellEta = icol;
              cellPhi = irow-AliEMCALGeoParams::fgkEMCALRows*int(cellSM/2);
            }

            //Count as bad "cells" out of EMCAL acceptance
            if(icol < 0 || icol > AliEMCALGeoParams::fgkEMCALCols*2 ||
               irow < 0 || irow > AliEMCALGeoParams::fgkEMCALRows*16./3) //5*nRows+1/3*nRows
            {
              coneCellsBad += 1.;
            }
            // Count as bad "cells" marked as bad in the DataBase
            else if (cu->GetEMCALChannelStatus(cellSM,cellEta,cellPhi,status)==1)
            {
              coneCellsBad += 1. ;
            }
          }
        }
      }//end of cells loop
    }
    else AliWarning("Cluster with bad (eta,phi) in EMCal for energy density calculation");

    if (coneCells > 0.)
    {
      cellDensity = (coneCells-coneCellsBad)/coneCells;
      //printf("Energy density = %f\n", cellDensity);
    }
  }

  return cellDensity;
}

//___________________________________________________________________________________
/// Get good cell density (number of active cells over all cells in cone).
//___________________________________________________________________________________
void AliIsolationCut::GetCoeffNormBadCell(AliCaloTrackParticleCorrelation * pCandidate,
                                          AliCaloTrackReader * reader,
                                          Float_t &  coneBadCellsCoeff,
                                          Float_t &  etaBandBadCellsCoeff,
                                          Float_t & phiBandBadCellsCoeff)
{
  Double_t coneCells    = 0.; //number of cells in cone with radius fConeSize
  Double_t phiBandCells = 0.; //number of cells in band phi
  Double_t etaBandCells = 0.; //number of cells in band eta

  Float_t phiC  = pCandidate->Phi() ;
  if ( phiC < 0 ) phiC+=TMath::TwoPi();
  Float_t etaC  = pCandidate->Eta() ;

  if(pCandidate->GetDetectorTag() == AliCaloTrackReader::kEMCAL)
  {
    AliEMCALGeometry* eGeom = AliEMCALGeometry::GetInstance();
    AliCalorimeterUtils *cu = reader->GetCaloUtils();

    Int_t absId = -999;
    if (eGeom->GetAbsCellIdFromEtaPhi(etaC,phiC,absId))
    {
      //Get absolute (col,row) of candidate
      Int_t iEta=-1, iPhi=-1, iRCU = -1;
      Int_t nSupMod = cu->GetModuleNumberCellIndexes(absId, pCandidate->GetDetectorTag(),
                                                     iEta, iPhi, iRCU);

      Int_t colC = iEta;
      if (nSupMod % 2) colC =  AliEMCALGeoParams::fgkEMCALCols + iEta ;
      Int_t rowC = iPhi + AliEMCALGeoParams::fgkEMCALRows*int(nSupMod/2);

      Int_t sqrSize = int(fConeSize/0.0143) ; // Size of cell in radians
      Int_t status  = 0;
      for(Int_t icol = 0; icol < 2*AliEMCALGeoParams::fgkEMCALCols-1;icol++)
      {
        for(Int_t irow = 0; irow < 5*AliEMCALGeoParams::fgkEMCALRows -1; irow++)
        {
          //loop on cells in a square of side fConeSize to check cells in cone
          if     ( Radius(colC, rowC, icol, irow) < sqrSize ) { coneCells    += 1.; }
          else if( icol>colC-sqrSize  &&  icol<colC+sqrSize ) { phiBandCells += 1 ; }
          else if( irow>rowC-sqrSize  &&  irow<rowC+sqrSize ) { etaBandCells += 1 ; }

          Int_t cellSM  = -999;
          Int_t cellEta = -999;
          Int_t cellPhi = -999;
          if(icol > AliEMCALGeoParams::fgkEMCALCols-1)
          {
            cellSM = 0+int(irow/AliEMCALGeoParams::fgkEMCALRows)*2;
            cellEta = icol-AliEMCALGeoParams::fgkEMCALCols;
            cellPhi = irow-AliEMCALGeoParams::fgkEMCALRows*int(cellSM/2);
          }
          if(icol < AliEMCALGeoParams::fgkEMCALCols)
          {
            cellSM = 1+int(irow/AliEMCALGeoParams::fgkEMCALRows)*2;
            cellEta = icol;
            cellPhi = irow-AliEMCALGeoParams::fgkEMCALRows*int(cellSM/2);
          }

          if( (icol < 0 || icol > AliEMCALGeoParams::fgkEMCALCols*2-1 ||
               irow < 0 || irow > AliEMCALGeoParams::fgkEMCALRows*5 - 1) //5*nRows+1/3*nRows //Count as bad "cells" out of EMCAL acceptance
             || (cu->GetEMCALChannelStatus(cellSM,cellEta,cellPhi,status)==1))  //Count as bad "cells" marked as bad in the DataBase
          {
            if     ( Radius(colC, rowC, icol, irow) < sqrSize ) coneBadCellsCoeff    += 1.;
            else if( icol>colC-sqrSize  &&  icol<colC+sqrSize ) phiBandBadCellsCoeff += 1 ;
	          else if( irow>rowC-sqrSize  &&  irow<rowC+sqrSize ) etaBandBadCellsCoeff += 1 ;
          }
        }
      }//end of cells loop
    }
    else AliWarning("Cluster with bad (eta,phi) in EMCal for energy density coeff calculation");

    if (coneCells > 0.)
    {
      //   printf("Energy density coneBadCellsCoeff= %.2f coneCells%.2f\n", coneBadCellsCoeff,coneCells);
      coneBadCellsCoeff = (coneCells-coneBadCellsCoeff)/coneCells;
      //  printf("coneBadCellsCoeff= %.2f\n", coneBadCellsCoeff);
    }
    
    if (phiBandCells > 0.)
    {
      // printf("Energy density phiBandBadCellsCoeff = %.2f phiBandCells%.2f\n", phiBandBadCellsCoeff,phiBandCells);
      phiBandBadCellsCoeff = (phiBandCells-phiBandBadCellsCoeff)/phiBandCells;
      // printf("phiBandBadCellsCoeff = %.2f\n", phiBandBadCellsCoeff);
    }
    
    if (etaBandCells > 0.)
    {
      //printf("Energy density etaBandBadCellsCoeff = %.2f etaBandCells%.2f\n", etaBandBadCellsCoeff,etaBandCells);
      etaBandBadCellsCoeff = (etaBandCells-etaBandBadCellsCoeff)/etaBandCells;
      // printf("etaBandBadCellsCoeff = %.2f\n",etaBandBadCellsCoeff);
    }
  }
}


//_________________________________________________________
/// Create histograms to be saved in output file and 
/// store them in outputContainer of the analysis class that calls this class.
//_________________________________________________________
TList * AliIsolationCut::GetCreateOutputObjects()
{    
  if ( !fHistoRanges )
  {
    AliFatal("Histogram ranges data base not initialized!");
    return 0x0;
  }
  
  fFillHistograms = kTRUE;

  TList * outputContainer = new TList() ; 
  outputContainer->SetName("IsolationCutBase") ; 
  outputContainer->SetOwner(kFALSE);

  Int_t   nptbins       = fHistoRanges->GetHistoPtBins();
  Int_t   nphibins      = fHistoRanges->GetHistoPhiBins();
  Int_t   netabins      = fHistoRanges->GetHistoEtaBins();
  Float_t ptmax         = fHistoRanges->GetHistoPtMax();
  Float_t phimax        = fHistoRanges->GetHistoPhiMax();
  Float_t etamax        = fHistoRanges->GetHistoEtaMax();
  Float_t ptmin         = fHistoRanges->GetHistoPtMin();
  Float_t phimin        = fHistoRanges->GetHistoPhiMin();
  Float_t etamin        = fHistoRanges->GetHistoEtaMin();
  
  Int_t   nptsumbins    = fHistoRanges->GetHistoNPtSumBins();
  Float_t ptsummax      = fHistoRanges->GetHistoPtSumMax();
  Float_t ptsummin      = fHistoRanges->GetHistoPtSumMin();
  Int_t   nptinconebins = fHistoRanges->GetHistoNPtInConeBins();
  Float_t ptinconemax   = fHistoRanges->GetHistoPtInConeMax();
  Float_t ptinconemin   = fHistoRanges->GetHistoPtInConeMin();
  
  TString sParticle = ", x^{0,#pm}";
  if      ( fPartInCone == kOnlyNeutral )  sParticle = ", x^{0}";
  else if ( fPartInCone == kOnlyCharged )  sParticle = ", x^{#pm}";
  
  TString parTitleR   = Form("#it{R} = %2.2f%s"       ,fConeSize,sParticle.Data());

  fhPtInCone  = new TH2F
  ("hPtInCone",
   Form("#it{p}_{T} of clusters and tracks in isolation cone for %s",parTitleR.Data()),
   nptbins,ptmin,ptmax,nptinconebins,ptinconemin,ptinconemax);
  fhPtInCone->SetYTitle("#it{p}_{T in cone} (GeV/#it{c})");
  fhPtInCone->SetXTitle("#it{p}_{T} (GeV/#it{c})");
  outputContainer->Add(fhPtInCone) ;
  
  fhConePtLead  = new TH2F
  ("hConePtLead",
   Form("Track or Cluster  leading #it{p}_{T} in isolation cone for #it{R} =  %2.2f",fConeSize),
   nptbins,ptmin,ptmax,nptbins,ptmin,ptmax);
  fhConePtLead->SetYTitle("#it{p}_{T, leading} (GeV/#it{c})");
  fhConePtLead->SetXTitle("#it{p}_{T, trigger} (GeV/#it{c})");
  outputContainer->Add(fhConePtLead) ;
  
  fhConeSumPt  = new TH2F
  ("hConePtSum",
   Form("Track and Cluster #Sigma #it{p}_{T} in isolation cone for #it{R} = %2.2f",fConeSize),
   nptbins,ptmin,ptmax,nptsumbins,ptsummin,ptsummax);
  fhConeSumPt->SetYTitle("#Sigma #it{p}_{T} (GeV/#it{c})");
  fhConeSumPt->SetXTitle("#it{p}_{T, trigger} (GeV/#it{c})");
  outputContainer->Add(fhConeSumPt) ;
    
  fhConeSumPtTrigEtaPhi  = new TH2F
  ("hConePtSumTrigEtaPhi",
   Form("Trigger #eta vs #varphi, #Sigma #it{p}_{T} in isolation cone for %s",parTitleR.Data()),
   netabins,etamin,etamax,nphibins,phimin,phimax);
  fhConeSumPtTrigEtaPhi->SetZTitle("#Sigma #it{p}_{T} (GeV/#it{c})");
  fhConeSumPtTrigEtaPhi->SetXTitle("#eta_{trigger}");
  fhConeSumPtTrigEtaPhi->SetYTitle("#varphi_{trigger} (rad)");
  outputContainer->Add(fhConeSumPtTrigEtaPhi) ;
  
  if ( fPartInCone != kOnlyCharged )
  {
    fhPtClusterInCone  = new TH2F
    ("hPtClusterInCone",
     Form("#it{p}_{T} of clusters in isolation cone for #it{R} =  %2.2f",fConeSize),
     nptbins,ptmin,ptmax,nptinconebins,ptinconemin,ptinconemax);
    fhPtClusterInCone->SetYTitle("#it{p}_{T in cone} (GeV/#it{c})");
    fhPtClusterInCone->SetXTitle("#it{p}_{T} (GeV/#it{c})");
    outputContainer->Add(fhPtClusterInCone) ;
    
    fhConePtLeadCluster  = new TH2F
    ("hConeLeadPtCluster",
     Form("Cluster leading in isolation cone for #it{R} =  %2.2f",fConeSize),
     nptbins,ptmin,ptmax,nptbins,ptmin,ptmax);
    fhConePtLeadCluster->SetYTitle("#it{p}_{T, leading} (GeV/#it{c})");
    fhConePtLeadCluster->SetXTitle("#it{p}_{T, trigger} (GeV/#it{c})");
    outputContainer->Add(fhConePtLeadCluster) ;
    
    fhConeSumPtCluster  = new TH2F
    ("hConePtSumCluster",
     Form("Cluster #Sigma #it{p}_{T} in isolation cone for #it{R} = %2.2f",fConeSize),
     nptbins,ptmin,ptmax,nptsumbins,ptsummin,ptsummax);
    fhConeSumPtCluster->SetYTitle("#Sigma #it{p}_{T} (GeV/#it{c})");
    fhConeSumPtCluster->SetXTitle("#it{p}_{T, trigger} (GeV/#it{c})");
    outputContainer->Add(fhConeSumPtCluster) ;
  }
  
  if ( fPartInCone != kOnlyNeutral )
  {
    fhPtTrackInCone  = new TH2F
    ("hPtTrackInCone",
     Form("#it{p}_{T} of tracks in isolation cone for #it{R} = %2.2f",fConeSize),
     nptbins,ptmin,ptmax,nptinconebins,ptinconemin,ptinconemax);
    fhPtTrackInCone->SetYTitle("#it{p}_{T in cone} (GeV/#it{c})");
    fhPtTrackInCone->SetXTitle("#it{p}_{T} (GeV/#it{c})");
    outputContainer->Add(fhPtTrackInCone) ;
    
    fhConePtLeadTrack  = new TH2F
    ("hConeLeadPtTrack",
     Form("Track leading in isolation cone for #it{R} = %2.2f",fConeSize),
     nptbins,ptmin,ptmax,nptbins,ptmin,ptmax);
    fhConePtLeadTrack->SetYTitle("#it{p}_{T, leading} (GeV/#it{c})");
    fhConePtLeadTrack->SetXTitle("#it{p}_{T, trigger} (GeV/#it{c})");
    outputContainer->Add(fhConePtLeadTrack) ;
    
    fhConeSumPtTrack  = new TH2F
    ("hConePtSumTrack",
     Form("Track #Sigma #it{p}_{T} in isolation cone for #it{R} =  %2.2f",fConeSize),
     nptbins,ptmin,ptmax,nptsumbins,ptsummin,ptsummax);
    fhConeSumPtTrack->SetYTitle("#Sigma #it{p}_{T} (GeV/#it{c})");
    fhConeSumPtTrack->SetXTitle("#it{p}_{T, trigger} (GeV/#it{c})");
    outputContainer->Add(fhConeSumPtTrack) ;
  }
  
  if ( fPartInCone == kNeutralAndCharged )
  {
    fhConeSumPtClustervsTrack   = new TH2F
    ("hConePtSumClustervsTrack",
     Form("Track vs Cluster #Sigma #it{p}_{T} in isolation cone for #it{R} =  %2.2f",fConeSize),
     nptsumbins,ptsummin,ptsummax,nptsumbins,ptsummin,ptsummax);
    fhConeSumPtClustervsTrack->SetXTitle("#Sigma #it{p}_{T}^{cluster} (GeV/#it{c})");
    fhConeSumPtClustervsTrack->SetYTitle("#Sigma #it{p}_{T}^{track} (GeV/#it{c})");
    outputContainer->Add(fhConeSumPtClustervsTrack) ;
    
    fhConeSumPtClusterTrackFrac   = new TH2F
    ("hConePtSumClusterTrackFraction",
     Form("#Sigma #it{p}_{T}^{cluster}/#Sigma #it{p}_{T}^{track} in isolation cone for #it{R} =  %2.2f",fConeSize),
     nptbins,ptmin,ptmax,200,0,5);
    fhConeSumPtClusterTrackFrac->SetYTitle("#Sigma #it{p}^{cluster}_{T} /#Sigma #it{p}_{T}^{track}");
    fhConeSumPtClusterTrackFrac->SetXTitle("#it{p}^{trigger}_{T} (GeV/#it{c})");
    outputContainer->Add(fhConeSumPtClusterTrackFrac) ;
    
    fhConePtLeadClustervsTrack   = new TH2F
    ("hConePtLeadClustervsTrack",
     Form("Track vs Cluster lead #it{p}_{T} in isolation cone for #it{R} =  %2.2f",fConeSize),
     nptbins,ptmin,ptmax,nptbins,ptmin,ptmax);
    fhConePtLeadClustervsTrack->SetXTitle("#it{p}^{leading cluster}_{T} (GeV/#it{c})");
    fhConePtLeadClustervsTrack->SetYTitle("#it{p}^{leading track}_{T} (GeV/#it{c})");
    outputContainer->Add(fhConePtLeadClustervsTrack) ;
    
    fhConePtLeadClusterTrackFrac   = new TH2F
    ("hConePtLeadClusterTrackFraction",
     Form(" #it{p}^{leading cluster}_{T}/#it{p}^{leading track}_{T} in isolation cone for #it{R} =  %2.2f",fConeSize),
     nptbins,ptmin,ptmax,200,0,5);
    fhConePtLeadClusterTrackFrac->SetYTitle("#it{p}^{leading cluster}_{T}/ #it{p}^{leading track}_{T}");
    fhConePtLeadClusterTrackFrac->SetXTitle("#it{p}^{trigger}_{T} (GeV/#it{c})");
    outputContainer->Add(fhConePtLeadClusterTrackFrac) ;
  }
  
  return outputContainer;
}

//____________________________________________
// Put data member values in string to keep
// in output container.
//____________________________________________
TString AliIsolationCut::GetICParametersList()
{
  TString parList ; //this will be list of parameters used for this analysis.
  const Int_t buffersize = 255;
  char onePar[buffersize] ;

  snprintf(onePar,buffersize,"--- AliIsolationCut ---\n") ;
  parList+=onePar ;
  snprintf(onePar,buffersize,"fConeSize: (isolation cone size) %1.2f\n",fConeSize) ;
  parList+=onePar ;
  snprintf(onePar,buffersize,"fPtThreshold >%2.2f;<%2.2f (isolation pt threshold) \n",fPtThreshold,fPtThresholdMax) ;
  parList+=onePar ;
  snprintf(onePar,buffersize,"fSumPtThreshold >%2.2f;<%2.2f (isolation sum pt threshold) \n",fSumPtThreshold,fSumPtThresholdMax) ;
  parList+=onePar ;
  snprintf(onePar,buffersize,"fPtFraction=%2.2f (isolation pt threshold fraction) \n",fPtFraction) ;
  parList+=onePar ;
  snprintf(onePar,buffersize,"fICMethod=%d (isolation cut case) \n",fICMethod) ;
  parList+=onePar ;
  snprintf(onePar,buffersize,"fPartInCone=%d \n",fPartInCone) ;
  parList+=onePar ;
  snprintf(onePar,buffersize,"fFracIsThresh=%i \n",fFracIsThresh) ;
  parList+=onePar ;
  snprintf(onePar,buffersize,"fDistMinToTrigger=%1.2f \n",fDistMinToTrigger) ;
  parList+=onePar ;

  return parList;
}

//____________________________________
// Initialize the parameters of the analysis.
//____________________________________
void AliIsolationCut::InitParameters()
{
  fFillHistograms = kFALSE; // True in GetCreateOutputObjects();
  fConeSize       = 0.4 ;
  fPtThreshold    = 0.5  ;
  fPtThresholdMax = 10000.  ;
  fSumPtThreshold    = 1.0 ;
  fSumPtThresholdMax = 10000. ;
  fPtFraction     = 0.1 ;
  fPartInCone     = kNeutralAndCharged;
  fICMethod       = kSumPtIC; // 0 pt threshol method, 1 cone pt sum method
  fFracIsThresh   = 1;
  fDistMinToTrigger = -1.; // no effect
}

//________________________________________________________________________________
/// Declare a candidate particle isolated depending on the
/// cluster or track particle multiplicity and/or momentum.
///
/// \param pCandidate: Kinematics and + of candidate particle for isolation.
/// \param reader: pointer to AliCaloTrackReader. Needed to access event info.
/// \param bFillAOD: Indicate if particles in cone must be added to AOD particle object.
/// \param useRefs: Get the list of tracks or clusters in cone from references
/// \param aodArrayRefName: Name of array where list of tracks/clusters in cone is stored.
/// \param bgTrk: List of tracks from mixed event background pool AliAnaParticleHadronCorrelation.
/// \param bgCls: List of clusters from mixed event background pool AliAnaParticleHadronCorrelation.
/// \param calorimeter: Which input trigger calorimeter used
/// \param pid: pointer to AliCaloPID. Needed to reject matched clusters in isolation cone.
/// \param nPart: number of tracks/clusters above threshold in cone, output.
/// \param nfrac: 1 if fraction pT cluster-track / pT trigger in cone avobe threshold, output.
/// \param coneptsum: total momentum energy in cone (track+cluster), output.
/// \param ptLead: momentum of leading cluster or track in cone, output.
/// \param isolated: final bool with decission on isolation of candidate particle.
/// \param histoWeight: Histograms weight (event, pt depedent).
///
//________________________________________________________________________________
void  AliIsolationCut::MakeIsolationCut
(
 AliCaloTrackParticleCorrelation  *pCandidate, AliCaloTrackReader * reader,
 Bool_t    bFillAOD,    Bool_t    useRefs, TString aodArrayRefName,
 TObjArray * bgTrk, TObjArray * bgCls,
 Int_t     calorimeter, AliCaloPID * pid,
 Int_t   & nPart      , Int_t   & nfrac,
 Float_t & coneptsum  , Float_t & ptLead,
 Bool_t  & isolated   , Double_t histoWeight   
)
{
  Float_t ptC   = pCandidate->Pt() ;
  Float_t phiC  = pCandidate->Phi() ;
  if ( phiC < 0 ) phiC+=TMath::TwoPi();
  Float_t etaC  = pCandidate->Eta() ;
  
  Float_t coneptsumCluster = 0;
  Float_t coneptsumTrack   = 0;
  Float_t coneptLeadCluster= 0;
  Float_t coneptLeadTrack  = 0;
  
  Float_t etaBandPtSumTrack   = 0;
  Float_t phiBandPtSumTrack   = 0;
  Float_t etaBandPtSumCluster = 0;
  Float_t phiBandPtSumCluster = 0;
  
  nPart     = 0 ;
  nfrac     = 0 ;
  isolated  = kFALSE;
  
  AliDebug(1,Form("Candidate pT %2.2f, eta %2.2f, phi %2.2f, cone %1.2f, thres %2.2f, Fill AOD? %d",
                  pCandidate->Pt(), pCandidate->Eta(), pCandidate->Phi()*TMath::RadToDeg(), 
                  fConeSize, fPtThreshold, bFillAOD));
  
  // --------------------------------
  // Get charged tracks and clusters in cone.
  // --------------------------------
  
  CalculateTrackSignalInCone   (pCandidate         , reader,
                                bFillAOD           , useRefs, 
                                aodArrayRefName    , bgTrk,
                                nPart              , nfrac,
                                coneptsumTrack     , coneptLeadTrack,
                                etaBandPtSumTrack  , phiBandPtSumTrack,
                                histoWeight);
  
  pCandidate->SetChargedLeadPtInCone(coneptLeadTrack);
  pCandidate->SetChargedPtSumInCone (coneptsumTrack);
  
  CalculateCaloSignalInCone    (pCandidate         , reader,
                                bFillAOD           , useRefs, 
                                aodArrayRefName    , bgCls,
                                calorimeter        , pid, 
                                nPart              , nfrac,
                                coneptsumCluster   , coneptLeadCluster,
                                etaBandPtSumCluster, phiBandPtSumCluster,
                                histoWeight);
  
  pCandidate->SetNeutralLeadPtInCone(coneptLeadCluster);
  pCandidate->SetNeutralPtSumInCone (coneptsumCluster);
  
  coneptsum = coneptsumCluster + coneptsumTrack;
  
  // *Now*, just check the leading particle in the cone if the threshold is passed
  if ( ptLead > fPtThreshold && ptLead < fPtThresholdMax)  nPart = 1;
  
  //if fPtFraction*ptC<fPtThreshold then consider the fPtThreshold directly
  if ( fFracIsThresh )
  {
    if ( fPtFraction*ptC < fPtThreshold )
    {
      if ( ptLead > fPtThreshold )    nfrac = 1 ;
    }
    else
    {
      if ( ptLead > fPtFraction*ptC ) nfrac = 1;
    }
  }
  else
  {
    if ( ptLead > fPtFraction*ptC )   nfrac = 1;
  }
  
  //-------------------------------------------------------------------
  // Check isolation, depending on selected isolation criteria requested
  //-------------------------------------------------------------------

  if ( fICMethod == kPtThresIC )
  {
    if ( nPart == 0 ) isolated = kTRUE ;
    
    AliDebug(1,Form("pT Cand %2.2f, pT Lead %2.2f, %2.2f<pT Lead< %2.2f, isolated %d",
                    ptC,ptLead,fPtThreshold,fPtThresholdMax,isolated));
  }
  else if ( fICMethod == kSumPtIC )
  {
    if ( coneptsum > fSumPtThreshold &&
         coneptsum < fSumPtThresholdMax )
      isolated  =  kFALSE ;
    else
      isolated  =  kTRUE  ;
    
    AliDebug(1,Form("pT Cand %2.2f, SumPt %2.2f, %2.2f<Sum pT< %2.2f, isolated %d",
                    ptC,ptLead,fSumPtThreshold,fSumPtThresholdMax,isolated));
  }
  else if ( fICMethod == kPtFracIC )
  {
    if ( nfrac == 0 ) isolated = kTRUE ;
  }
  else if ( fICMethod == kSumPtFracIC )
  {
    //when the fPtFraction*ptC < fSumPtThreshold then consider the later case
    // printf("photon analysis IsDataMC() ?%i\n",IsDataMC());
    if ( fFracIsThresh )
    {
      if( fPtFraction*ptC < fSumPtThreshold  && coneptsum < fSumPtThreshold ) isolated  =  kTRUE ;
      if( fPtFraction*ptC > fSumPtThreshold  && coneptsum < fPtFraction*ptC ) isolated  =  kTRUE ;
    }
    else
    {
      if ( coneptsum < fPtFraction*ptC ) isolated  =  kTRUE ;
    }
  }
  else if ( fICMethod == kSumDensityIC )
  {
    // Get good cell density (number of active cells over all cells in cone)
    // and correct energy in cone
    
    Float_t cellDensity = GetCellDensity(pCandidate,reader);
    
    if ( coneptsum < fSumPtThreshold*cellDensity )
      isolated = kTRUE;
  }
  else if ( fICMethod == kSumBkgSubIC )
  {
    Double_t coneptsumBkg = 0.;
    Float_t  etaBandPtSumTrackNorm   = 0;
    Float_t  phiBandPtSumTrackNorm   = 0;
    Float_t  etaBandPtSumClusterNorm = 0;
    Float_t  phiBandPtSumClusterNorm = 0;
    
    Float_t  excessFracEtaTrack   = 1;
    Float_t  excessFracPhiTrack   = 1;
    Float_t  excessFracEtaCluster = 1;
    Float_t  excessFracPhiCluster = 1;
    
    // Normalize background to cone area
    if     ( fPartInCone != kOnlyCharged )
      CalculateUEBandClusterNormalization(reader, etaC, phiC,
                                          phiBandPtSumCluster    , etaBandPtSumCluster,
                                          phiBandPtSumClusterNorm, etaBandPtSumClusterNorm,
                                          excessFracEtaCluster   , excessFracPhiCluster    );
    
    if     ( fPartInCone != kOnlyNeutral )
      CalculateUEBandTrackNormalization(reader, etaC, phiC,
                                        phiBandPtSumTrack    , etaBandPtSumTrack  ,
                                        phiBandPtSumTrackNorm, etaBandPtSumTrackNorm,
                                        excessFracEtaTrack   , excessFracPhiTrack    );
    
    if      ( fPartInCone == kOnlyCharged       ) coneptsumBkg = etaBandPtSumTrackNorm;
    else if ( fPartInCone == kOnlyNeutral       ) coneptsumBkg = etaBandPtSumClusterNorm;
    else if ( fPartInCone == kNeutralAndCharged ) coneptsumBkg = etaBandPtSumClusterNorm + etaBandPtSumTrackNorm;
    
    //coneptsumCluster*=(coneBadCellsCoeff*excessFracEtaCluster*excessFracPhiCluster) ; // apply this correction earlier???
    // line commented out in last modif!!!
    
    coneptsum = coneptsumCluster+coneptsumTrack;
    
    coneptsum -= coneptsumBkg;
    
    if ( coneptsum > fSumPtThreshold && coneptsum < fSumPtThresholdMax )
      isolated  =  kFALSE ;
    else
      isolated  =  kTRUE  ;
    
  }
  
  //-------------------------------------------------------------------
  // Fill histograms
  //-------------------------------------------------------------------

  if ( !fFillHistograms ) return;
  
  if ( fPartInCone == kNeutralAndCharged )
  {
    fhConeSumPtClustervsTrack ->Fill(coneptsumCluster, coneptsumTrack , histoWeight);
    fhConePtLeadClustervsTrack->Fill(coneptLeadCluster,coneptLeadTrack, histoWeight);
    
    if(coneptsumTrack  > 0) fhConeSumPtClusterTrackFrac ->Fill(ptC, coneptsumCluster /coneptsumTrack , histoWeight);
    if(coneptLeadTrack > 0) fhConePtLeadClusterTrackFrac->Fill(ptC, coneptLeadCluster/coneptLeadTrack, histoWeight);
  }
  
  fhConeSumPt              ->Fill(ptC,        coneptsum, histoWeight);
  fhConeSumPtTrigEtaPhi    ->Fill(etaC, phiC, coneptsum *histoWeight); // check

  Float_t coneptLead = coneptLeadTrack;
  if(coneptLeadCluster > coneptLeadTrack) 
    coneptLead = coneptLeadCluster;
  
  fhConePtLead->Fill(ptC, coneptLead, histoWeight);
  
}

//_____________________________________________________
/// Print some relevant parameters set for the analysis.
//_____________________________________________________
void AliIsolationCut::Print(const Option_t * opt) const
{
  if(! opt)
    return;

  printf("**** Print %s %s **** \n", GetName(), GetTitle() ) ;

  printf("IC method          =     %d\n",    fICMethod   ) ;
  printf("Cone Size          =     %1.2f\n", fConeSize   ) ;
  printf("pT threshold       =     >%2.1f;<%2.1f\n", fPtThreshold   ,   fPtThresholdMax) ;
  printf("Sum pT threshold   =     >%2.1f;<%2.1f\n", fSumPtThreshold,fSumPtThresholdMax) ;
  printf("pT fraction        =     %3.1f\n", fPtFraction ) ;
  printf("particle type in cone =  %d\n",    fPartInCone ) ;
  printf("using fraction for high pt leading instead of frac ? %i\n",fFracIsThresh);
  printf("minimum distance to candidate, R>%1.2f\n",fDistMinToTrigger);
  printf("    \n") ;
}

//______________________________________________________________
/// Calculate the distance to trigger from any particle.
/// \param etaC: pseudorapidity of candidate particle.
/// \param phiC: azimuthal angle of candidate particle.
/// \param eta: pseudorapidity of track/cluster to be considered in cone.
/// \param phi: azimuthal angle of track/cluster to be considered in cone.
//______________________________________________________________
Float_t AliIsolationCut::Radius(Float_t etaC, Float_t phiC,
                                Float_t eta , Float_t phi) const
{
  Float_t dEta = etaC-eta;
  Float_t dPhi = phiC-phi;
  
  if(TMath::Abs(dPhi) >= TMath::Pi())
    dPhi = TMath::TwoPi()-TMath::Abs(dPhi);

  return TMath::Sqrt( dEta*dEta + dPhi*dPhi );
}



