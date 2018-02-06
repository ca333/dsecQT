/******************************************************************************
 * Copyright © 2014-2017 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef H_KOMODO_H
#define H_KOMODO_H

// Todo:
// verify: reorgs

#define KOMODO_ASSETCHAINS_WAITNOTARIZE
#define KOMODO_PAXMAX (10000 * COIN)

#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <ctype.h>
#include "uthash.h"
#include "utlist.h"
#include "init.h"

int32_t gettxout_scriptPubKey(uint8_t *scriptPubkey,int32_t maxsize,uint256 txid,int32_t n);
void komodo_event_rewind(struct komodo_state *sp,char *symbol,int32_t height);
void komodo_connectblock(CBlockIndex *pindex,CBlock& block);

#include "komodo_structs.h"
#include "komodo_globals.h"
#include "komodo_utils.h"
#include "komodo_curve25519.h"

#include "cJSON.c"
#include "komodo_komodod.h"
#include "komodo_interest.h"
#include "komodo_pax.h"
#include "komodo_notary.h"

int32_t komodo_parsestatefile(struct komodo_state *sp,FILE *fp,char *symbol,char *dest);
int32_t komodo_parsestatefile(struct komodo_state *sp,char *symbol,char *dest);
#include "komodo_kv.h"
#include "komodo_jumblr.h"
#include "komodo_gateway.h"
#include "komodo_events.h"

void komodo_currentheight_set(int32_t height)
{
    char symbol[16],dest[16]; struct komodo_state *sp;
    if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        sp->CURRENT_HEIGHT = height;
}

int32_t komodo_currentheight()
{
    char symbol[16],dest[16]; struct komodo_state *sp;
    if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        return(sp->CURRENT_HEIGHT);
    else return(0);
}

int32_t komodo_parsestatefile(struct komodo_state *sp,FILE *fp,char *symbol,char *dest)
{
    static int32_t errs;
    int32_t func,ht,notarized_height,num,matched=0; uint256 notarized_hash,notarized_desttxid; uint8_t pubkeys[64][33];
    if ( (func= fgetc(fp)) != EOF )
    {
        if ( ASSETCHAINS_SYMBOL[0] == 0 && strcmp(symbol,"KMD") == 0 )
            matched = 1;
        else matched = (strcmp(symbol,ASSETCHAINS_SYMBOL) == 0);
        if ( fread(&ht,1,sizeof(ht),fp) != sizeof(ht) )
            errs++;
        //LogPrintf("fpos.%ld func.(%d %c) ht.%d ",ftell(fp),func,func,ht);
        if ( func == 'P' )
        {
            if ( (num= fgetc(fp)) <= 64 )
            {
                if ( fread(pubkeys,33,num,fp) != num )
                    errs++;
                else
                {
                    //LogPrintf("updated %d pubkeys at %s ht.%d\n",num,symbol,ht);
                    if ( (KOMODO_EXTERNAL_NOTARIES != 0 && matched != 0) || (strcmp(symbol,"KMD") == 0 && KOMODO_EXTERNAL_NOTARIES == 0) )
                        komodo_eventadd_pubkeys(sp,symbol,ht,num,pubkeys);
                }
            } else LogPrintf("illegal num.%d\n",num);
        }
        else if ( func == 'N' )
        {
            if ( fread(&notarized_height,1,sizeof(notarized_height),fp) != sizeof(notarized_height) )
                errs++;
            if ( fread(&notarized_hash,1,sizeof(notarized_hash),fp) != sizeof(notarized_hash) )
                errs++;
            if ( fread(&notarized_desttxid,1,sizeof(notarized_desttxid),fp) != sizeof(notarized_desttxid) )
                errs++;
            if ( 0 && sp != 0 )
                LogPrintf("%s load[%s.%d] NOTARIZED %d %s\n",ASSETCHAINS_SYMBOL,symbol,sp->NUM_NPOINTS,notarized_height,notarized_hash.ToString().c_str());
            //if ( matched != 0 ) global independent states -> inside *sp
            komodo_eventadd_notarized(sp,symbol,ht,dest,notarized_hash,notarized_desttxid,notarized_height);
        }
        else if ( func == 'U' ) // deprecated
        {
            uint8_t n,nid; uint256 hash; uint64_t mask;
            n = fgetc(fp);
            nid = fgetc(fp);
            //LogPrintf("U %d %d\n",n,nid);
            if ( fread(&mask,1,sizeof(mask),fp) != sizeof(mask) )
                errs++;
            if ( fread(&hash,1,sizeof(hash),fp) != sizeof(hash) )
                errs++;
            //if ( matched != 0 )
            //    komodo_eventadd_utxo(sp,symbol,ht,nid,hash,mask,n);
        }
        else if ( func == 'K' )
        {
            int32_t kheight;
            if ( fread(&kheight,1,sizeof(kheight),fp) != sizeof(kheight) )
                errs++;
            //if ( matched != 0 ) global independent states -> inside *sp
            //LogPrintf("%s.%d load[%s] ht.%d\n",ASSETCHAINS_SYMBOL,ht,symbol,kheight);
            komodo_eventadd_kmdheight(sp,symbol,ht,kheight,0);
        }
        else if ( func == 'T' )
        {
            int32_t kheight,ktimestamp;
            if ( fread(&kheight,1,sizeof(kheight),fp) != sizeof(kheight) )
                errs++;
            if ( fread(&ktimestamp,1,sizeof(ktimestamp),fp) != sizeof(ktimestamp) )
                errs++;
            //if ( matched != 0 ) global independent states -> inside *sp
            //LogPrintf("%s.%d load[%s] ht.%d t.%u\n",ASSETCHAINS_SYMBOL,ht,symbol,kheight,ktimestamp);
            komodo_eventadd_kmdheight(sp,symbol,ht,kheight,ktimestamp);
        }
        else if ( func == 'R' )
        {
            uint16_t olen,v; uint64_t ovalue; uint256 txid; uint8_t opret[16384];
            if ( fread(&txid,1,sizeof(txid),fp) != sizeof(txid) )
                errs++;
            if ( fread(&v,1,sizeof(v),fp) != sizeof(v) )
                errs++;
            if ( fread(&ovalue,1,sizeof(ovalue),fp) != sizeof(ovalue) )
                errs++;
            if ( fread(&olen,1,sizeof(olen),fp) != sizeof(olen) )
                errs++;
            if ( olen < sizeof(opret) )
            {
                if ( fread(opret,1,olen,fp) != olen )
                    errs++;
                if ( 0 && matched != 0 )
                {
                    int32_t i;  for (i=0; i<olen; i++)
                        LogPrintf("%02x",opret[i]);
                    LogPrintf(" %s.%d load[%s] opret[%c] len.%d %.8f\n",ASSETCHAINS_SYMBOL,ht,symbol,opret[0],olen,(double)ovalue/COIN);
                }
                komodo_eventadd_opreturn(sp,symbol,ht,txid,ovalue,v,opret,olen); // global shared state -> global PAX
            } else
            {
                int32_t i;
                for (i=0; i<olen; i++)
                    fgetc(fp);
                //LogPrintf("illegal olen.%u\n",olen);
            }
        }
        else if ( func == 'D' )
        {
            LogPrintf("unexpected function D[%d]\n",ht);
        }
        else if ( func == 'V' )
        {
            int32_t numpvals; uint32_t pvals[128];
            numpvals = fgetc(fp);
            if ( numpvals*sizeof(uint32_t) <= sizeof(pvals) && fread(pvals,sizeof(uint32_t),numpvals,fp) == numpvals )
            {
                //if ( matched != 0 ) global shared state -> global PVALS
                //LogPrintf("%s load[%s] prices %d\n",ASSETCHAINS_SYMBOL,symbol,ht);
                komodo_eventadd_pricefeed(sp,symbol,ht,pvals,numpvals);
                //LogPrintf("load pvals ht.%d numpvals.%d\n",ht,numpvals);
            } else LogPrintf("error loading pvals[%d]\n",numpvals);
        }
        else LogPrintf("[%s] %s illegal func.(%d %c)\n",ASSETCHAINS_SYMBOL,symbol,func,func);
        return(func);
    } else return(-1);
}

unsigned char *mp;
int32_t mpos;
int32_t fsize;

int32_t komodo_parsestatefile(struct komodo_state *sp,char *symbol,char *dest)
{
    static int32_t errs;
    int32_t func,ht,notarized_height,num,matched=0; uint256 notarized_hash,notarized_desttxid; uint8_t pubkeys[64][33];
    if ( mpos < fsize )
    {
        func = mp[mpos++];

        if ( ASSETCHAINS_SYMBOL[0] == 0 && strcmp(symbol,"KMD") == 0 )
            matched = 1;
        else matched = (strcmp(symbol,ASSETCHAINS_SYMBOL) == 0);

        if ( mpos+sizeof(ht) > fsize ) errs++;
        else
        {
            memcpy(&ht,&mp[mpos],sizeof(ht));
            mpos+=sizeof(ht);
        }
        //LogPrintf("fpos.%ld func.(%d %c) ht.%d ",ftell(fp),func,func,ht);
        if ( func == 'P' )
        {
            if ( (num = mp[mpos++]) <= 64 )
            {
                if ( mpos+33*num > fsize ) errs++;
                else
                {
                    memcpy(pubkeys,&mp[mpos],33*num);
                    mpos+=33*num;

                    //LogPrintf("updated %d pubkeys at %s ht.%d\n",num,symbol,ht);
                    if ( (KOMODO_EXTERNAL_NOTARIES != 0 && matched != 0) || (strcmp(symbol,"KMD") == 0 && KOMODO_EXTERNAL_NOTARIES == 0) )
                        komodo_eventadd_pubkeys(sp,symbol,ht,num,pubkeys);
                }
            } else LogPrintf("illegal num.%d\n",num);
        }
        else if ( func == 'N' )
        {
            if ( mpos+sizeof(notarized_height) > fsize ) errs++;
            else
            {
                memcpy(&notarized_height,&mp[mpos],sizeof(notarized_height));
                mpos+=sizeof(notarized_height);
            }

            if ( mpos+sizeof(notarized_hash) > fsize ) errs++;
            else
            {
                memcpy(&notarized_hash,&mp[mpos],sizeof(notarized_hash));
                mpos+=sizeof(notarized_hash);
            }

            if ( mpos+sizeof(notarized_desttxid) > fsize ) errs++;
            else
            {
                memcpy(&notarized_desttxid,&mp[mpos],sizeof(notarized_desttxid));
                mpos+=sizeof(notarized_desttxid);
            }

            if ( 0 && sp != 0 )
                LogPrintf("%s load[%s.%d] NOTARIZED %d %s\n",ASSETCHAINS_SYMBOL,symbol,sp->NUM_NPOINTS,notarized_height,notarized_hash.ToString().c_str());
            //if ( matched != 0 ) global independent states -> inside *sp
            komodo_eventadd_notarized(sp,symbol,ht,dest,notarized_hash,notarized_desttxid,notarized_height);
        }
        else if ( func == 'U' ) // deprecated
        {
            uint8_t n,nid; uint256 hash; uint64_t mask;
            n = mp[mpos++];
            nid = mp[mpos++];

            //LogPrintf("U %d %d\n",n,nid);
            if ( mpos+sizeof(mask) > fsize ) errs++;
            else
            {
                memcpy(&mask,&mp[mpos],sizeof(mask));
                mpos+=sizeof(mask);
            }

            if ( mpos+sizeof(hash) > fsize ) errs++;
            else
            {
                memcpy(&hash,&mp[mpos],sizeof(hash));
                mpos+=sizeof(hash);
            }

            //if ( matched != 0 )
            //    komodo_eventadd_utxo(sp,symbol,ht,nid,hash,mask,n);
        }
        else if ( func == 'K' )
        {
            int32_t kheight;
            if ( mpos+sizeof(kheight) > fsize ) errs++;
            else
            {
                memcpy(&kheight,&mp[mpos],sizeof(kheight));
                mpos+=sizeof(kheight);
            }

            //if ( matched != 0 ) global independent states -> inside *sp
            //LogPrintf("%s.%d load[%s] ht.%d\n",ASSETCHAINS_SYMBOL,ht,symbol,kheight);
            komodo_eventadd_kmdheight(sp,symbol,ht,kheight,0);
        }
        else if ( func == 'T' )
        {
            int32_t kheight,ktimestamp;
            if ( mpos+sizeof(kheight) > fsize ) errs++;
            else
            {
                memcpy(&kheight,&mp[mpos],sizeof(kheight));
                mpos+=sizeof(kheight);
            }

            if ( mpos+sizeof(ktimestamp) > fsize ) errs++;
            else
            {
                memcpy(&ktimestamp,&mp[mpos],sizeof(ktimestamp));
                mpos+=sizeof(ktimestamp);
            }

            //if ( matched != 0 ) global independent states -> inside *sp
            //LogPrintf("%s.%d load[%s] ht.%d t.%u\n",ASSETCHAINS_SYMBOL,ht,symbol,kheight,ktimestamp);
            komodo_eventadd_kmdheight(sp,symbol,ht,kheight,ktimestamp);
        }
        else if ( func == 'R' )
        {
            uint16_t olen,v; uint64_t ovalue; uint256 txid; uint8_t opret[16384];
            if ( mpos+sizeof(txid) > fsize ) errs++;
            else
            {
                memcpy(&txid,&mp[mpos],sizeof(txid));
                mpos+=sizeof(txid);
            }

            if ( mpos+sizeof(v) > fsize ) errs++;
            else
            {
                memcpy(&v,&mp[mpos],sizeof(v));
                mpos+=sizeof(v);
            }

            if ( mpos+sizeof(ovalue) > fsize ) errs++;
            else
            {
                memcpy(&ovalue,&mp[mpos],sizeof(ovalue));
                mpos+=sizeof(ovalue);
            }

            if ( mpos+sizeof(olen) > fsize ) errs++;
            else
            {
                memcpy(&olen,&mp[mpos],sizeof(olen));
                mpos+=sizeof(olen);
            }

            if ( olen < sizeof(opret) )
            {
                if ( mpos+olen > fsize ) errs++;
                else
                {
                    memcpy(opret,&mp[mpos],olen);
                    mpos+=olen;
                }

                if ( 0 && matched != 0 )
                {
                    int32_t i;  for (i=0; i<olen; i++)
                        LogPrintf("%02x",opret[i]);
                    LogPrintf(" %s.%d load[%s] opret[%c] len.%d %.8f\n",ASSETCHAINS_SYMBOL,ht,symbol,opret[0],olen,(double)ovalue/COIN);
                }
                komodo_eventadd_opreturn(sp,symbol,ht,txid,ovalue,v,opret,olen); // global shared state -> global PAX
            } else
            {
                mpos+=olen;
                //LogPrintf("illegal olen.%u\n",olen);
            }
        }
        else if ( func == 'D' )
        {
            LogPrintf("unexpected function D[%d]\n",ht);
        }
        else if ( func == 'V' )
        {
            int32_t numpvals; uint32_t pvals[128];
            numpvals = mp[mpos++];

            if ( numpvals*sizeof(uint32_t) <= sizeof(pvals) && mpos+numpvals*sizeof(uint32_t) <= fsize)
            {
                memcpy(pvals,&mp[mpos],numpvals*sizeof(uint32_t));
                mpos+=numpvals*sizeof(uint32_t);

                //if ( matched != 0 ) global shared state -> global PVALS
                //LogPrintf("%s load[%s] prices %d\n",ASSETCHAINS_SYMBOL,symbol,ht);
                komodo_eventadd_pricefeed(sp,symbol,ht,pvals,numpvals);
                //LogPrintf("load pvals ht.%d numpvals.%d\n",ht,numpvals);
            } else LogPrintf("error loading pvals[%d]\n",numpvals);
        }
        else LogPrintf("[%s] %s illegal func.(%d %c)\n",ASSETCHAINS_SYMBOL,symbol,func,func);
        return(func);
    } else return(-1);
}

void komodo_stateupdate(int32_t height,uint8_t notarypubs[][33],uint8_t numnotaries,uint8_t notaryid,uint256 txhash,uint64_t voutmask,uint8_t numvouts,uint32_t *pvals,uint8_t numpvals,int32_t KMDheight,uint32_t KMDtimestamp,uint64_t opretvalue,uint8_t *opretbuf,uint16_t opretlen,uint16_t vout)
{
    static FILE *fp; static int32_t errs,didinit;
    struct komodo_state *sp; char fname[512],symbol[16],dest[16]; int32_t ht,func; uint8_t num,pubkeys[64][33];
    if ( didinit == 0 )
    {
        portable_mutex_init(&KOMODO_KV_mutex);
        didinit = 1;
    }
    if ( (sp= komodo_stateptr(symbol,dest)) == 0 )
    {
        KOMODO_INITDONE = (uint32_t)time(NULL);
        return;
    }
    if ( fp == 0 )
    {
        komodo_statefname(fname,ASSETCHAINS_SYMBOL,(char *)"komodostate");
        if ( (fp= fopen(fname,"rb+")) != 0 )
        {
            fseek(fp,0,SEEK_END);
            fsize = (int32_t)ftell(fp);
            rewind(fp);

            if (fsize != 0)
            {
                mp = (unsigned char*)malloc(fsize);
                if (mp)
                {
                    //!!!!!!!!! turned off...
		    //fread(mp,1,fsize,fp);
                    mpos = 0;

                    LogPrintf("%s: komodostate loaded...\n", __func__);

//                    while ( komodo_parsestatefile(sp,fp,symbol,dest) >= 0 )

//!!!!!!!!! turned off...
//                    while ( komodo_parsestatefile(sp,symbol,dest) >= 0 )
//                    {
//                        if (ShutdownRequested()) break;
//                    }

                    free(mp);

                    LogPrintf("%s: komodostate parsed...\n", __func__);
                }
            }
        } else fp = fopen(fname,"wb+");
        LogPrintf("fname.(%s) fpos.%ld\n",fname,ftell(fp));
        KOMODO_INITDONE = (uint32_t)time(NULL);
    }
    if ( height <= 0 )
    {
        //LogPrintf("early return: stateupdate height.%d\n",height);
        return;
    }
    if ( fp != 0 ) // write out funcid, height, other fields, call side effect function
    {
        //LogPrintf("fpos.%ld ",ftell(fp));
        if ( KMDheight != 0 )
        {
            if ( KMDtimestamp != 0 )
            {
                fputc('T',fp);
                if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                    errs++;
                if ( fwrite(&KMDheight,1,sizeof(KMDheight),fp) != sizeof(KMDheight) )
                    errs++;
                if ( fwrite(&KMDtimestamp,1,sizeof(KMDtimestamp),fp) != sizeof(KMDtimestamp) )
                    errs++;
            }
            else
            {
                fputc('K',fp);
                if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                    errs++;
                if ( fwrite(&KMDheight,1,sizeof(KMDheight),fp) != sizeof(KMDheight) )
                    errs++;
            }
            komodo_eventadd_kmdheight(sp,symbol,height,KMDheight,KMDtimestamp);
        }
        else if ( opretbuf != 0 && opretlen > 0 )
        {
            uint16_t olen = opretlen;
            fputc('R',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            if ( fwrite(&txhash,1,sizeof(txhash),fp) != sizeof(txhash) )
                errs++;
            if ( fwrite(&vout,1,sizeof(vout),fp) != sizeof(vout) )
                errs++;
            if ( fwrite(&opretvalue,1,sizeof(opretvalue),fp) != sizeof(opretvalue) )
                errs++;
            if ( fwrite(&olen,1,sizeof(olen),fp) != olen )
                errs++;
            if ( fwrite(opretbuf,1,olen,fp) != olen )
                errs++;
//LogPrintf("ht.%d R opret[%d] sp.%p\n",height,olen,sp);
            //komodo_opreturn(height,opretvalue,opretbuf,olen,txhash,vout);
            komodo_eventadd_opreturn(sp,symbol,height,txhash,opretvalue,vout,opretbuf,olen);
        }
        else if ( notarypubs != 0 && numnotaries > 0 )
        {
            LogPrintf("ht.%d func P[%d] errs.%d\n",height,numnotaries,errs);
            fputc('P',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            fputc(numnotaries,fp);
            if ( fwrite(notarypubs,33,numnotaries,fp) != numnotaries )
                errs++;
            komodo_eventadd_pubkeys(sp,symbol,height,numnotaries,notarypubs);
        }
        else if ( voutmask != 0 && numvouts > 0 )
        {
            //LogPrintf("ht.%d func U %d %d errs.%d hashsize.%ld\n",height,numvouts,notaryid,errs,sizeof(txhash));
            fputc('U',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            fputc(numvouts,fp);
            fputc(notaryid,fp);
            if ( fwrite(&voutmask,1,sizeof(voutmask),fp) != sizeof(voutmask) )
                errs++;
            if ( fwrite(&txhash,1,sizeof(txhash),fp) != sizeof(txhash) )
                errs++;
            //komodo_eventadd_utxo(sp,symbol,height,notaryid,txhash,voutmask,numvouts);
        }
        else if ( pvals != 0 && numpvals > 0 )
        {
            int32_t i,nonz = 0;
            for (i=0; i<32; i++)
                if ( pvals[i] != 0 )
                    nonz++;
            if ( nonz >= 32 )
            {
                fputc('V',fp);
                if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                    errs++;
                fputc(numpvals,fp);
                if ( fwrite(pvals,sizeof(uint32_t),numpvals,fp) != numpvals )
                    errs++;
                komodo_eventadd_pricefeed(sp,symbol,height,pvals,numpvals);
                //LogPrintf("ht.%d V numpvals[%d]\n",height,numpvals);
            }
            //LogPrintf("save pvals height.%d numpvals.%d\n",height,numpvals);
        }
        else if ( height != 0 )
        {
            //LogPrintf("ht.%d func N ht.%d errs.%d\n",height,NOTARIZED_HEIGHT,errs);
            if ( sp != 0 )
            {
                fputc('N',fp);
                if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                    errs++;
                if ( fwrite(&sp->NOTARIZED_HEIGHT,1,sizeof(sp->NOTARIZED_HEIGHT),fp) != sizeof(sp->NOTARIZED_HEIGHT) )
                    errs++;
                if ( fwrite(&sp->NOTARIZED_HASH,1,sizeof(sp->NOTARIZED_HASH),fp) != sizeof(sp->NOTARIZED_HASH) )
                    errs++;
                if ( fwrite(&sp->NOTARIZED_DESTTXID,1,sizeof(sp->NOTARIZED_DESTTXID),fp) != sizeof(sp->NOTARIZED_DESTTXID) )
                    errs++;
                komodo_eventadd_notarized(sp,symbol,height,dest,sp->NOTARIZED_HASH,sp->NOTARIZED_DESTTXID,sp->NOTARIZED_HEIGHT);
            }
        }
        fflush(fp);
    }
}

int32_t komodo_voutupdate(int32_t *isratificationp,int32_t notaryid,uint8_t *scriptbuf,int32_t scriptlen,int32_t height,uint256 txhash,int32_t i,int32_t j,uint64_t *voutmaskp,int32_t *specialtxp,int32_t *notarizedheightp,uint64_t value,int32_t notarized,uint64_t signedmask)
{
    static uint256 zero; static FILE *signedfp;
    int32_t opretlen,nid,k,len = 0; uint256 kmdtxid,desttxid; uint8_t crypto777[33]; struct komodo_state *sp; char symbol[16],dest[16];
    if ( (sp= komodo_stateptr(symbol,dest)) == 0 )
        return(-1);
    if ( scriptlen == 35 && scriptbuf[0] == 33 && scriptbuf[34] == 0xac )
    {
        if ( i == 0 && j == 0 && memcmp(NOTARY_PUBKEY33,scriptbuf+1,33) == 0 && NOTARY_PUBKEY33[0] != 0 )
        {
            LogPrintf("%s KOMODO_LASTMINED.%d -> %d\n",ASSETCHAINS_SYMBOL,KOMODO_LASTMINED,height);
            prevKOMODO_LASTMINED = KOMODO_LASTMINED;
            KOMODO_LASTMINED = height;
        }
        decode_hex(crypto777,33,(char *)CRYPTO777_PUBSECPSTR);
        /*for (k=0; k<33; k++)
            LogPrintf("%02x",crypto777[k]);
        LogPrintf(" crypto777 ");
        for (k=0; k<scriptlen; k++)
            LogPrintf("%02x",scriptbuf[k]);
        LogPrintf(" <- script ht.%d i.%d j.%d cmp.%d\n",height,i,j,memcmp(crypto777,scriptbuf+1,33));*/
        if ( memcmp(crypto777,scriptbuf+1,33) == 0 )
        {
            *specialtxp = 1;
            //LogPrintf(">>>>>>>> ");
        }
        else if ( komodo_chosennotary(&nid,height,scriptbuf + 1) >= 0 )
        {
            //LogPrintf("found notary.k%d\n",k);
            if ( notaryid < 64 )
            {
                if ( notaryid < 0 )
                {
                    notaryid = nid;
                    *voutmaskp |= (1LL << j);
                }
                else if ( notaryid != nid )
                {
                    //for (i=0; i<33; i++)
                    //    LogPrintf("%02x",scriptbuf[i+1]);
                    //LogPrintf(" %s mismatch notaryid.%d k.%d\n",ASSETCHAINS_SYMBOL,notaryid,nid);
                    notaryid = 64;
                    *voutmaskp = 0;
                }
                else *voutmaskp |= (1LL << j);
            }
        }
    }
    if ( scriptbuf[len++] == 0x6a )
    {
        if ( (opretlen= scriptbuf[len++]) == 0x4c )
            opretlen = scriptbuf[len++];
        else if ( opretlen == 0x4d )
        {
            opretlen = scriptbuf[len++];
            opretlen += (scriptbuf[len++] << 8);
        }
        if ( j == 1 && opretlen >= 32*2+4 && strcmp(ASSETCHAINS_SYMBOL[0]==0?"KMD":ASSETCHAINS_SYMBOL,(char *)&scriptbuf[len+32*2+4]) == 0 )
        {
            len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&kmdtxid);
            len += iguana_rwnum(0,&scriptbuf[len],sizeof(*notarizedheightp),(uint8_t *)notarizedheightp);
            len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&desttxid);
            if ( notarized != 0 && *notarizedheightp > sp->NOTARIZED_HEIGHT && *notarizedheightp < height && (height < sp->CURRENT_HEIGHT-1000 || komodo_verifynotarization(ASSETCHAINS_SYMBOL[0]==0?(char *)"KMD":ASSETCHAINS_SYMBOL,(char *)(ASSETCHAINS_SYMBOL[0] == 0 ? "BTC" : "KMD"),height,*notarizedheightp,kmdtxid,desttxid) == 0) )
            {
                sp->NOTARIZED_HEIGHT = *notarizedheightp;
                sp->NOTARIZED_HASH = kmdtxid;
                sp->NOTARIZED_DESTTXID = desttxid;
                komodo_stateupdate(height,0,0,0,zero,0,0,0,0,0,0,0,0,0,0);
                len += 4;
                if ( 0 && ASSETCHAINS_SYMBOL[0] == 0 )
                    LogPrintf("%s ht.%d NOTARIZED.%d %s.%s %sTXID.%s (%s) lens.(%d %d)\n",ASSETCHAINS_SYMBOL,height,*notarizedheightp,ASSETCHAINS_SYMBOL[0]==0?"KMD":ASSETCHAINS_SYMBOL,kmdtxid.ToString().c_str(),ASSETCHAINS_SYMBOL[0]==0?"BTC":"KMD",desttxid.ToString().c_str(),(char *)&scriptbuf[len],opretlen,len);
                if ( ASSETCHAINS_SYMBOL[0] == 0 )
                {
                    if ( signedfp == 0 )
                    {
                        char fname[512];
                        komodo_statefname(fname,(char *)"",(char *)"signedmasks");
                        if ( (signedfp= fopen(fname,"rb+")) == 0 )
                            signedfp = fopen(fname,"wb");
                        else fseek(signedfp,0,SEEK_END);
                    }
                    if ( signedfp != 0 )
                    {
                        fwrite(&height,1,sizeof(height),signedfp);
                        fwrite(&signedmask,1,sizeof(signedmask),signedfp);
                        fflush(signedfp);
                    }
                    if ( opretlen > len && scriptbuf[len] == 'A' )
                    {
                        //for (i=0; i<opretlen-len; i++)
                        //    LogPrintf("%02x",scriptbuf[len+i]);
                        //LogPrintf(" Found extradata.[%d] %d - %d\n",opretlen-len,opretlen,len);
                        komodo_stateupdate(height,0,0,0,txhash,0,0,0,0,0,0,value,&scriptbuf[len],opretlen-len+4+3+(scriptbuf[1] == 0x4d),j);
                    }
                }
            } else if ( height >= KOMODO_MAINNET_START )
                LogPrintf("notarized.%d %llx reject ht.%d NOTARIZED.%d prev.%d %s.%s DESTTXID.%s (%s)\n",notarized,(long long)signedmask,height,*notarizedheightp,sp->NOTARIZED_HEIGHT,ASSETCHAINS_SYMBOL[0]==0?"KMD":ASSETCHAINS_SYMBOL,kmdtxid.ToString().c_str(),desttxid.ToString().c_str(),(char *)&scriptbuf[len]);
        }
        else if ( i == 0 && j == 1 && opretlen == 149 )
        {
            if ( notaryid >= 0 && notaryid < 64 )
                komodo_paxpricefeed(height,&scriptbuf[len],opretlen);
        }
        else
        {
            //int32_t k; for (k=0; k<scriptlen; k++)
            //    LogPrintf("%02x",scriptbuf[k]);
            //LogPrintf(" <- script ht.%d i.%d j.%d value %.8f %s\n",height,i,j,dstr(value),ASSETCHAINS_SYMBOL);
            if ( opretlen >= 32*2+4 && strcmp(ASSETCHAINS_SYMBOL[0]==0?"KMD":ASSETCHAINS_SYMBOL,(char *)&scriptbuf[len+32*2+4]) == 0 )
            {
                for (k=0; k<32; k++)
                    if ( scriptbuf[len+k] != 0 )
                        break;
                if ( k == 32 )
                {
                    *isratificationp = 1;
                    LogPrintf("ISRATIFICATION (%s)\n",(char *)&scriptbuf[len+32*2+4]);
                }
            }
            
            if ( *isratificationp == 0 && (signedmask != 0 || (scriptbuf[len] != 'X' && scriptbuf[len] != 'A')) ) // && scriptbuf[len] != 'I')
                komodo_stateupdate(height,0,0,0,txhash,0,0,0,0,0,0,value,&scriptbuf[len],opretlen,j);
        }
    }
    return(notaryid);
}

/*int32_t komodo_isratify(int32_t isspecial,int32_t numvalid)
{
    if ( isspecial != 0 && numvalid >= KOMODO_MINRATIFY )
        return(1);
    else return(0);
}*/

// Special tx have vout[0] -> CRYPTO777
// with more than KOMODO_MINRATIFY pay2pubkey outputs -> ratify
// if all outputs to notary -> notary utxo
// if txi == 0 && 2 outputs and 2nd OP_RETURN, len == 32*2+4 -> notarized, 1st byte 'P' -> pricefeed
// OP_RETURN: 'D' -> deposit, 'W' -> withdraw

int32_t gettxout_scriptPubKey(uint8_t *scriptPubKey,int32_t maxsize,uint256 txid,int32_t n);

int32_t komodo_notarycmp(uint8_t *scriptPubKey,int32_t scriptlen,uint8_t pubkeys[64][33],int32_t numnotaries,uint8_t rmd160[20])
{
    int32_t i;
    if ( scriptlen == 25 && memcmp(&scriptPubKey[3],rmd160,20) == 0 )
        return(0);
    else if ( scriptlen == 35 )
    {
        for (i=0; i<numnotaries; i++)
            if ( memcmp(&scriptPubKey[1],pubkeys[i],33) == 0 )
                return(i);
    }
    return(-1);
}

void komodo_connectblock(CBlockIndex *pindex,CBlock& block)
{
    static int32_t hwmheight;
    uint64_t signedmask,voutmask; char symbol[16],dest[16]; struct komodo_state *sp;
    uint8_t scriptbuf[4096],pubkeys[64][33],rmd160[20],scriptPubKey[35]; uint256 kmdtxid,zero,btctxid,txhash;
    int32_t i,j,k,numnotaries,notarized,scriptlen,isratification,nid,numvalid,specialtx,notarizedheight,notaryid,len,numvouts,numvins,height,txn_count;
    memset(&zero,0,sizeof(zero));
    komodo_init(pindex->nHeight);
    KOMODO_INITDONE = (uint32_t)time(NULL);
    if ( (sp= komodo_stateptr(symbol,dest)) == 0 )
    {
        LogPrintf("unexpected null komodostateptr.[%s]\n",ASSETCHAINS_SYMBOL);
        return;
    }
    //LogPrintf("%s connect.%d\n",ASSETCHAINS_SYMBOL,pindex->nHeight);
    numnotaries = komodo_notaries(pubkeys,pindex->nHeight);
    calc_rmd160_sha256(rmd160,pubkeys[0],33);
    if ( pindex->nHeight > hwmheight )
        hwmheight = pindex->nHeight;
    else
    {
        if ( pindex->nHeight != hwmheight )
            LogPrintf("%s hwmheight.%d vs pindex->nHeight.%d t.%u reorg.%d\n",ASSETCHAINS_SYMBOL,hwmheight,pindex->nHeight,(uint32_t)pindex->nTime,hwmheight-pindex->nHeight);
        komodo_event_rewind(sp,symbol,pindex->nHeight);
        komodo_stateupdate(pindex->nHeight,0,0,0,zero,0,0,0,0,-pindex->nHeight,pindex->nTime,0,0,0,0);
    }
    komodo_currentheight_set(chainActive.Tip()->nHeight);
    if ( pindex != 0 )
    {
        height = pindex->nHeight;
        txn_count = block.vtx.size();
        for (i=0; i<txn_count; i++)
        {
            txhash = block.vtx[i].GetHash();
            numvouts = block.vtx[i].vout.size();
            notaryid = -1;
            voutmask = specialtx = notarizedheight = isratification = notarized = 0;
            signedmask = (height < 91400) ? 1 : 0;
            numvins = block.vtx[i].vin.size();
            for (j=0; j<numvins; j++)
            {
                if ( i == 0 && j == 0 )
                    continue;
                if ( (scriptlen= gettxout_scriptPubKey(scriptPubKey,sizeof(scriptPubKey),block.vtx[i].vin[j].prevout.hash,block.vtx[i].vin[j].prevout.n)) > 0 )
                {
                    if ( (k= komodo_notarycmp(scriptPubKey,scriptlen,pubkeys,numnotaries,rmd160)) >= 0 )
                        signedmask |= (1LL << k);
                    else if ( 0 && numvins >= 17 )
                    {
                        int32_t k;
                        for (k=0; k<scriptlen; k++)
                            LogPrintf("%02x",scriptPubKey[k]);
                        LogPrintf(" scriptPubKey doesnt match any notary vini.%d of %d\n",j,numvins);
                    }
                } else LogPrintf("cant get scriptPubKey for ht.%d txi.%d vin.%d\n",height,i,j);
            }
            numvalid = bitweight(signedmask);
            if ( (((height < 90000 || (signedmask & 1) != 0) && numvalid >= KOMODO_MINRATIFY) || numvalid > (numnotaries/5)) )
            {
                LogPrintf("%s ht.%d txi.%d signedmask.%llx numvins.%d numvouts.%d <<<<<<<<<<< notarized\n",ASSETCHAINS_SYMBOL,height,i,(long long)signedmask,numvins,numvouts);
                notarized = 1;
            }
            if ( NOTARY_PUBKEY33[0] != 0 && ASSETCHAINS_SYMBOL[0] == 0 )
                LogPrintf("(tx.%d: ",i);
            for (j=0; j<numvouts; j++)
            {
                /*if ( i == 0 && j == 0 )
                {
                    uint8_t *script = (uint8_t *)block.vtx[0].vout[numvouts-1].scriptPubKey.data();
                    if ( numvouts <= 2 || script[0] != 0x6a )
                    {
                        if ( numvouts == 2 && block.vtx[0].vout[1].nValue != 0 )
                        {
                            LogPrintf("ht.%d numvouts.%d value %.8f\n",height,numvouts,dstr(block.vtx[0].vout[1].nValue));
                            if ( height >= 235300 && block.vtx[0].vout[1].nValue >= 100000*COIN )
                                block.vtx[0].vout[1].nValue = 0;
                            break;
                        }
                    }
                }*/
                if ( NOTARY_PUBKEY33[0] != 0 && ASSETCHAINS_SYMBOL[0] == 0 )
                    LogPrintf("%.8f ",dstr(block.vtx[i].vout[j].nValue));
                len = block.vtx[i].vout[j].scriptPubKey.size();
                if ( len >= sizeof(uint32_t) && len <= sizeof(scriptbuf) )
                {
#ifdef KOMODO_ZCASH
                    memcpy(scriptbuf,block.vtx[i].vout[j].scriptPubKey.data(),len);
#else
                    memcpy(scriptbuf,(uint8_t *)&block.vtx[i].vout[j].scriptPubKey[0],len);
#endif
                    notaryid = komodo_voutupdate(&isratification,notaryid,scriptbuf,len,height,txhash,i,j,&voutmask,&specialtx,&notarizedheight,(uint64_t)block.vtx[i].vout[j].nValue,notarized,signedmask);
                    if ( 0 && i > 0 )
                    {
                        for (k=0; k<len; k++)
                            LogPrintf("%02x",scriptbuf[k]);
                        LogPrintf(" <- notaryid.%d ht.%d i.%d j.%d numvouts.%d numvins.%d voutmask.%llx txid.(%s)\n",notaryid,height,i,j,numvouts,numvins,(long long)voutmask,txhash.ToString().c_str());
                    }
                }
            }
            if ( NOTARY_PUBKEY33[0] != 0 && ASSETCHAINS_SYMBOL[0] == 0 )
                LogPrintf(") ");
            //LogPrintf("%s ht.%d txi.%d signedmask.%llx numvins.%d numvouts.%d notarized.%d special.%d isratification.%d\n",ASSETCHAINS_SYMBOL,height,i,(long long)signedmask,numvins,numvouts,notarized,specialtx,isratification);
            if ( notarized != 0 && (notarizedheight != 0 || specialtx != 0) )
            {
                if ( isratification != 0 )
                {
                    LogPrintf("%s NOTARY SIGNED.%llx numvins.%d ht.%d txi.%d notaryht.%d specialtx.%d\n",ASSETCHAINS_SYMBOL,(long long)signedmask,numvins,height,i,notarizedheight,specialtx);
                    LogPrintf("ht.%d specialtx.%d isratification.%d numvouts.%d signed.%llx numnotaries.%d\n",height,specialtx,isratification,numvouts,(long long)signedmask,numnotaries);
                }
                if ( specialtx != 0 && isratification != 0 && numvouts > 2 )
                {
                    numvalid = 0;
                    memset(pubkeys,0,sizeof(pubkeys));
                    for (j=1; j<numvouts-1; j++)
                    {
                        len = block.vtx[i].vout[j].scriptPubKey.size();
                        if ( len >= sizeof(uint32_t) && len <= sizeof(scriptbuf) )
                        {
#ifdef KOMODO_ZCASH
                            memcpy(scriptbuf,block.vtx[i].vout[j].scriptPubKey.data(),len);
#else
                            memcpy(scriptbuf,(uint8_t *)&block.vtx[i].vout[j].scriptPubKey[0],len);
#endif
                            if ( len == 35 && scriptbuf[0] == 33 && scriptbuf[34] == 0xac )
                            {
                                memcpy(pubkeys[numvalid++],scriptbuf+1,33);
                                for (k=0; k<33; k++)
                                    LogPrintf("%02x",scriptbuf[k+1]);
                                LogPrintf(" <- new notary.[%d]\n",j-1);
                            }
                        }
                    }
                    if ( ((signedmask & 1) != 0 && numvalid >= KOMODO_MINRATIFY) || bitweight(signedmask) > (numnotaries/3) )
                    {
                        memset(&txhash,0,sizeof(txhash));
                        komodo_stateupdate(height,pubkeys,numvalid,0,txhash,0,0,0,0,0,0,0,0,0,0);
                        LogPrintf("RATIFIED! >>>>>>>>>> new notaries.%d newheight.%d from height.%d\n",numvalid,(((height+KOMODO_ELECTION_GAP/2)/KOMODO_ELECTION_GAP)+1)*KOMODO_ELECTION_GAP,height);
                    } else LogPrintf("signedmask.%llx numvalid.%d wt.%d numnotaries.%d\n",(long long)signedmask,numvalid,bitweight(signedmask),numnotaries);
                }
            }
        }
        if ( NOTARY_PUBKEY33[0] != 0 && ASSETCHAINS_SYMBOL[0] == 0 )
            LogPrintf("%s ht.%d\n",ASSETCHAINS_SYMBOL[0] == 0 ? "KMD" : ASSETCHAINS_SYMBOL,height);
        if ( pindex->nHeight == hwmheight )
            komodo_stateupdate(height,0,0,0,zero,0,0,0,0,height,(uint32_t)pindex->nTime,0,0,0,0);
    } else LogPrintf("komodo_connectblock: unexpected null pindex\n");
    //KOMODO_INITDONE = (uint32_t)time(NULL);
    //LogPrintf("%s end connect.%d\n",ASSETCHAINS_SYMBOL,pindex->nHeight);
}


#endif