
/*Edited by Tomás Lagos*/

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "LoWPAN_HC.h"
#include "IPv6_checksum.h"

char *reassemble_lowpan(lowpan_header *lowpan, int payload_lenght, char *buffer)
{
    uint8_t returnValueBase;
    int i,j,packet_size = 0;
    int lowpan_src_lenght = 0,lowpan_dst_lenght=0;

    RoHC_base *aux = (RoHC_base *)malloc(sizeof(RoHC_base));

    aux->tfn = IPHC_TF_ELIDED;

    aux->nh = IPHC_NH_INLINE;

    if (lowpan->hlim == 1)
    {
        aux->hlim = IPHC_HLIM_1;
        lowpan->hlim = 0;
    }
    else if(lowpan->hlim == 64)
    {
        aux->hlim = IPHC_HLIM_64;
        lowpan->hlim = 0;
    }
    else if (lowpan->hlim == 255)
    {
        aux->hlim = IPHC_HLIM_255;
        lowpan->hlim = 0; 
    }
    else
    {
        aux->hlim = IPHC_HLIM_255; 
    }    
      ///////////////////////////////////////////////////////////

    aux->cid = IPHC_CID_NO;


    aux->sac = IPHC_SAC_STATELESS;


    if(sizeof(lowpan->src) == 16)
    {   
        aux->sam = IPHC_SAM_128B;
    }
    else if (sizeof(lowpan->src) == 8)
    {
        aux->sam = IPHC_SAM_64B;
    }
    else if (sizeof(lowpan->src) == 2)
    {
        aux->sam = IPHC_SAM_16B;
    }
    else if (sizeof(lowpan->src) == 0)
    {
        aux->sam = IPHC_SAM_ELIDED;
    }    

    aux->m = IPHC_M_NO;
    aux->dac = IPHC_DAC_STATELESS;

    if(sizeof(lowpan->dst) == 16)
    {
        aux->dam = IPHC_DAM_128B;
    }
    else if (sizeof(lowpan->dst) == 8)
    {
        aux->dam = IPHC_DAM_64B;
    }
    else if (sizeof(lowpan->dst) == 2)
    {
        aux->dam = IPHC_DAM_16B;
    }
    else if (sizeof(lowpan->dst) == 0)
    {
        aux->dam = IPHC_DAM_ELIDED;
    }
    returnValueBase = 0;
    returnValueBase |= (aux->hlim         << 0);
    returnValueBase |= (aux->nh           << 2);
    returnValueBase |= (aux->tfn          << 3);
    returnValueBase |= (IPHC_DISPATCH     << 5);
    returnValueBase |= (PAGE_ONE_DISPATCH << 7);

    buffer[0] = returnValueBase;
    packet_size++;

    returnValueBase = 0;
    returnValueBase |= (aux->dam          << 0); 
    returnValueBase |= (aux->dac          << 2);
    returnValueBase |= (aux->m            << 3);
    returnValueBase |= (aux->sam          << 4);
    returnValueBase |= (aux->sac          << 6);
    returnValueBase |= (aux->cid          << 7);
    
    buffer[1] = returnValueBase;
    packet_size++;

    buffer[2] = lowpan->nh;
    packet_size++;

    j=3;

    lowpan_src_lenght =  sizeof(lowpan->src.slowpan_addr);
    lowpan_dst_lenght =  sizeof(lowpan->dst.slowpan_addr);

    for(i= 0; i < lowpan_src_lenght; i++)
    {
        buffer[j] = lowpan->src.slowpan_addr[i];
        packet_size++;
        j++;
    }
    for(i= 0; i < lowpan_dst_lenght; i++)
    {
        buffer[j] = lowpan->dst.slowpan_addr[i];
        packet_size++;
        j++;
    }
    for(i=0 ; i < payload_lenght; i++)
    {
        buffer[j] = lowpan->payload.slowpan_payload[i];
        packet_size++;
        j++;
    }

    if(packet_size > 64)
    {
        free(aux);
        return NULL;
    }
    else
    {
        free(aux);
        return buffer;    
    }
}

char *IPv6ToMesh(char *buffer, int lenght_buffer, lowpan_header *lowpanh)
{
    ip6_buffer *iph = (ip6_buffer *)buffer;

    if (iph->ip6_dst.s6_addr[0] == 0xff)
    {
      return NULL;
    }
    else
    {
        lowpanh->nh = iph->ip6_ctlun.ip6_un1.ip6_un1_nxt;
        lowpanh->hlim = iph->ip6_ctlun.ip6_un1.ip6_un1_hlim;

        int ip6_src_len = sizeof(iph->ip6_src);
        int ip6_dst_len = sizeof(iph->ip6_dst);
        int payload_len = lenght_buffer - 40;

        memcpy(lowpanh->src.slowpan_addr,&iph->ip6_src.s6_addr[0],ip6_src_len);

        if(ip6_dst_len == 16)
        {
            memcpy(lowpanh->dst.slowpan_addr,&iph->ip6_dst.s6_addr[8],ip6_dst_len);
        }

        memcpy(lowpanh->payload.slowpan_payload,&buffer[40],payload_len);

        buffer = reassemble_lowpan(lowpanh, payload_len, buffer);
        
        if( buffer != NULL)
        {
            return buffer;    
        }
        else
        {
            return NULL;
        }   
    }
}


ip6_buffer *DecodeIPv6(char *buffer, int lenght_buffer ,ip6_buffer *decIPv6)
{
    int i,j,k;

    RoHC_base *HRbase = (RoHC_base *)malloc(sizeof(RoHC_base));

    HRbase->tfn= (buffer[0] >> 3) & 3;
    HRbase->nh = (buffer[0] >> 2) & 1; 
    HRbase->hlim = buffer[0] & 3; 
    HRbase->cid = (buffer[1] >> 7) & 1;
    HRbase->sac = (buffer[1] >> 6) & 1;
    HRbase->sam = (buffer[1] >> 4) & 3;
    HRbase->m = (buffer[1] >> 3) & 1;
    HRbase->dac = (buffer[1] >> 2) & 1;
    HRbase->dam = buffer[1] & 3;

    decIPv6->ip6_ctlun.ip6_un1.ip6_un1_flow = 0x60;
    if(HRbase->tfn == 3)
    {
        j = 2;
    }
    if(HRbase->nh == 0)
    {
        decIPv6->ip6_ctlun.ip6_un1.ip6_un1_nxt =buffer[2] ;
        j++;
    }
    if(HRbase->hlim == 2)
    {
        decIPv6->ip6_ctlun.ip6_un1.ip6_un1_hlim = 64;
    }
    if(HRbase->cid == 1)
    {
       j += 8;
    }
    if(HRbase->sam == 0)
    {
        if(HRbase->sac == 0)
        {
            k = j;
            memcpy(decIPv6->ip6_src.s6_addr,&buffer[k],16);
            j += 16;
        }
    }
    if(HRbase->m == 0 && HRbase->dac == 0)
    {
        if(HRbase->dam == 1)
        {
            k = j;
            decIPv6->ip6_dst.s6_addr[0] = 0xbb;
            decIPv6->ip6_dst.s6_addr[1] = 0xbb;

            for(i = 2; i < 8; i++)
            {
                decIPv6->ip6_dst.s6_addr[i] = 0;
            }
            for (i = 0; i < 8; i++)
            {
                decIPv6->ip6_dst.s6_addr[i+8] = buffer[i+k];
                j++;
            }    
        }
    }
    k=j;

    decIPv6->ip6_ctlun.ip6_un1.ip6_un1_plen = 0 ;
    decIPv6->ip6_ctlun.ip6_un1.ip6_un1_plen |= (lenght_buffer - k) << 8 ;

    memcpy(decIPv6->payload.s6_payload,&buffer[k],decIPv6->ip6_ctlun.ip6_un1.ip6_un1_plen);
   
    free(HRbase);
    return decIPv6;
}


char *IPv6Rx(char *buffer, int lenght_buffer, int tun_fd, ip6_buffer *decode)
{
    int j = 0; 
    char buffer_decoded[1500];
    uint16_t gen_checksum;
    char *ptr;
    uint16_t payload_len;
    ip6_buffer *auxRX;

    ptr = &buffer_decoded[0];

    decode = DecodeIPv6(buffer, lenght_buffer, decode);
        
    if(decode->ip6_ctlun.ip6_un1.ip6_un1_nxt == 58)
    {
        if(decode->payload.s6_payload[0] == 0x80)
        {
            decode->payload.s6_payload[0] = 0x81;
            auxRX = decode;
            decode->ip6_src = decode->ip6_dst;
            decode->ip6_dst = auxRX->ip6_src; 

            memcpy(ptr, &decode->ip6_ctlun.ip6_un1.ip6_un1_flow, sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_flow));
            ptr += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_flow);  
            j += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_flow);  

            payload_len = decode->ip6_ctlun.ip6_un1.ip6_un1_plen >> 8 & 255;
            memcpy(ptr, &decode->ip6_ctlun.ip6_un1.ip6_un1_plen, sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_plen));
            ptr += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_plen);  
            j += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_plen);

            memcpy(ptr, &decode->ip6_ctlun.ip6_un1.ip6_un1_nxt, sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_nxt));
            ptr += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_nxt);  
            j += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_nxt);

            memcpy(ptr, &decode->ip6_ctlun.ip6_un1.ip6_un1_hlim, sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_hlim));
            ptr += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_hlim);  
            j += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_hlim);

            memcpy(ptr, &decode->ip6_src.s6_addr, sizeof (decode->ip6_src.s6_addr));
            ptr += sizeof (decode->ip6_src.s6_addr);
            j += sizeof (decode->ip6_src.s6_addr);    

            memcpy(ptr, &decode->ip6_dst.s6_addr, payload_len);
            ptr += sizeof (decode->ip6_dst.s6_addr);
            j += sizeof (decode->ip6_dst.s6_addr);

            memcpy(ptr, &decode->payload.s6_payload, sizeof(decode->payload.s6_payload));
            ptr += payload_len;
            j += payload_len; 

            gen_checksum = checksum_icmpv6(buffer_decoded,(j));
            buffer_decoded[42] = (gen_checksum >> 8) & 255;
            buffer_decoded[43] = (gen_checksum) & 255;

            memcpy(buffer,&buffer_decoded[0],j);
            
            return buffer;

        }
        else
        {
            memcpy(ptr, &decode->ip6_ctlun.ip6_un1.ip6_un1_flow, sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_flow));
            ptr += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_flow);  
            j += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_flow);  

            payload_len = decode->ip6_ctlun.ip6_un1.ip6_un1_plen >> 8 & 255;
            memcpy(ptr, &decode->ip6_ctlun.ip6_un1.ip6_un1_plen, sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_plen));
            ptr += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_plen);  
            j += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_plen);

            memcpy(ptr, &decode->ip6_ctlun.ip6_un1.ip6_un1_nxt, sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_nxt));
            ptr += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_nxt);  
            j += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_nxt);

            memcpy(ptr, &decode->ip6_ctlun.ip6_un1.ip6_un1_hlim, sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_hlim));
            ptr += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_hlim);  
            j += sizeof (decode->ip6_ctlun.ip6_un1.ip6_un1_hlim);

            memcpy(ptr, &decode->ip6_src.s6_addr, sizeof (decode->ip6_src.s6_addr));
            ptr += sizeof (decode->ip6_src.s6_addr);
            j += sizeof (decode->ip6_src.s6_addr);    

            memcpy(ptr, &decode->ip6_dst.s6_addr, payload_len);
            ptr += sizeof (decode->ip6_dst.s6_addr);
            j += sizeof (decode->ip6_dst.s6_addr);

            memcpy(ptr, &decode->payload.s6_payload, sizeof(decode->payload.s6_payload));
            ptr += payload_len;
            j += payload_len; 

            write(tun_fd, buffer_decoded, (decode->ip6_ctlun.ip6_un1.ip6_un1_plen + 40));
        }    
    }
    return NULL;
}
