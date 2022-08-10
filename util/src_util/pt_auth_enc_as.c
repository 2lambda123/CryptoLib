/* Copyright (C) 2009 - 2022 National Aeronautics and Space Administration.
   All Foreign Rights are Reserved to the U.S. Government.

   This software is provided "as is" without any warranty of any kind, either expressed, implied, or statutory,
   including, but not limited to, any warranty that the software will conform to specifications, any implied warranties
   of merchantability, fitness for a particular purpose, and freedom from infringement, and any warranty that the
   documentation will conform to the program, or any warranty that the software will be error free.

   In no event shall NASA be liable for any damages, including, but not limited to direct, indirect, special or
   consequential damages, arising out of, resulting from, or in any way connected with the software or its
   documentation, whether or not based upon warranty, contract, tort or otherwise, and whether or not loss was sustained
   from, or arose out of the results of, or use of, the software, documentation or services provided hereunder.

   ITC Team
   NASA IV&V
   jstar-development-team@mail.nasa.gov
*/

/**
 *  Unit Tests that make use of TC_ApplySecurity/TC_ProcessSecurity function on the data with KMC Crypto Service/MariaDB Functionality Enabled.
 **/
#include "crypto.h"
#include "crypto_error.h"
#include "sadb_routine.h"
#include "utest.h"

#include "crypto.h"
#include "shared_util.h"
#include <stdio.h>

#include <time.h>
#include <unistd.h>

int num_frames_1K = 1000;
int num_frames_100 = 100;

void Write_To_File(uint16_t enc_frame_len, float total_time, char* test_name, int num_frames, int reset)
{
    if(reset == 1)
    {
        if(access("PERFORMANCE_RESULTS_AEAS.csv", F_OK) == 0)
        {
            int deleted = remove("PERFORMANCE_RESULTS_AEAS.csv");
            if(deleted){printf("ERROR Deleting File!\n");}
        }
    }

    FILE *fp = NULL;
    fp = fopen("PERFORMANCE_RESULTS_AEAS.csv", "a");
    if (fp != NULL)
    {   
        if(reset ==1) fprintf(fp, "Name of Test,Frames Sent,Bytes per Frame,Total Time,Mbps\n");
        fprintf(fp, "%s,%d,%d,%f,%f\n", test_name, num_frames, enc_frame_len, total_time, (((enc_frame_len * 8 * num_frames)/total_time)/1024/1024));
    }
    fclose(fp);
    
}

double Apply_Security_Loop(uint8_t *frame, int frame_length, uint8_t *enc_frame, uint16_t *enc_frame_len, int num_loops)
{
    struct timespec begin, end;
    double total_time;
    total_time = 0.0;

    frame = frame;
    frame_length = frame_length;
    enc_frame_len = enc_frame_len;

    int32_t status = CRYPTO_LIB_SUCCESS;

    for(int i = 0; i < num_loops; i++)
    {
        printf("LOOP NUMBER: %d\n", i+1);
        clock_gettime(CLOCK_REALTIME, &begin);
        status = Crypto_TC_ApplySecurity(frame, frame_length, &enc_frame, enc_frame_len);
        clock_gettime(CLOCK_REALTIME, &end);
        free(enc_frame);

        long seconds = end.tv_sec - begin.tv_sec;
        long nanoseconds = end.tv_nsec - begin.tv_nsec;
        double elapsed = seconds + nanoseconds*1e-9;

        if (status != CRYPTO_LIB_SUCCESS)
        {
            total_time = -1.0;
            printf("ERROR: %d\n", status);
            break;
        }

        total_time += elapsed;
    }
    return total_time;
}

UTEST(PERFORMANCE, LSA_LIBG_SHORT_100)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init_Unit_Test();
    char* raw_tc_sdls_ping_h = "202C046100ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEFF01C";
                                
    char* raw_tc_sdls_ping_b = NULL;
    int raw_tc_sdls_ping_len = 0;
    SadbRoutine sadb_routine = get_sadb_routine_inmemory();

    float ttl_time_lsa_clib_100 = 0.0;

    hex_conversion(raw_tc_sdls_ping_h, &raw_tc_sdls_ping_b, &raw_tc_sdls_ping_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    SecurityAssociation_t* test_association = malloc(sizeof(SecurityAssociation_t) * sizeof(uint8_t));
    // Expose the SADB Security Association for test edits.
    sadb_routine->sadb_get_sa_from_spi(1, &test_association);
    test_association->sa_state = SA_NONE;
    sadb_routine->sadb_get_sa_from_spi(10, &test_association);
    test_association->sa_state = SA_OPERATIONAL;
    test_association->arsn_len = 0;

    ttl_time_lsa_clib_100 =
        Apply_Security_Loop((uint8_t* )raw_tc_sdls_ping_b, raw_tc_sdls_ping_len, ptr_enc_frame, &enc_frame_len, num_frames_100);

    printf("\nLSA+LIBG Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_100);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_lsa_clib_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_100)/ttl_time_lsa_clib_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_lsa_clib_100, "LSA+LIBG Apply Security SHORT", num_frames_100, 1);

    Crypto_Shutdown();
    free(raw_tc_sdls_ping_b);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, MDB_LIBG_SHORT_100)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_MARIADB, CRYPTOGRAPHY_TYPE_LIBGCRYPT, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_short = "202C046100ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEFF01C";
    
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_short, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_100);
    
    printf("\nMDB+LIBG Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_100);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_100)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "MDB+LIBG Apply Security SHORT", num_frames_100, 0);
    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, LSA_KMC_SHORT_100)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_KMCCRYPTO, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_Kmc_Crypto_Service("https", "client-demo-kmc.example.com", 8443, "crypto-service","/home/itc/Desktop/CERTS/ammos-ca-bundle.crt",NULL, CRYPTO_FALSE, "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "PEM","/home/itc/Desktop/CERTS/ammos-client-key.pem", NULL, NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_short = "202C046100ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEFF01C"; 
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_short, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_100);
    
    printf("\nLSA+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_100);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_100)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "LSA+KMC Apply Security SHORT", num_frames_100, 0);
    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, MDB_KMC_SHORT_100)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_MARIADB, CRYPTOGRAPHY_TYPE_KMCCRYPTO, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Kmc_Crypto_Service("https", "client-demo-kmc.example.com", 8443, "crypto-service","/home/itc/Desktop/CERTS/ammos-ca-bundle.crt",NULL, CRYPTO_FALSE, "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "PEM","/home/itc/Desktop/CERTS/ammos-client-key.pem", NULL, NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    
    char* raw_tc_jpl_mmt_scid44_vcid1_short = "202C046100ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEFF01C";
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;

    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_short, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_100);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_100);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_100)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "MDB+KMC Apply Security SHORT", num_frames_100, 0);
    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, LSA_LIBG_MED_100)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init_Unit_Test();
    char* raw_tc_sdls_ping_h = "202C05E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b00313011";
                                
    char* raw_tc_sdls_ping_b = NULL;
    int raw_tc_sdls_ping_len = 0;
    SadbRoutine sadb_routine = get_sadb_routine_inmemory();

    float ttl_time_lsa_clib_100 = 0.0;

    hex_conversion(raw_tc_sdls_ping_h, &raw_tc_sdls_ping_b, &raw_tc_sdls_ping_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    SecurityAssociation_t* test_association = malloc(sizeof(SecurityAssociation_t) * sizeof(uint8_t));
    // Expose the SADB Security Association for test edits.
    sadb_routine->sadb_get_sa_from_spi(1, &test_association);
    test_association->sa_state = SA_NONE;
    sadb_routine->sadb_get_sa_from_spi(10, &test_association);
    test_association->sa_state = SA_OPERATIONAL;
    test_association->arsn_len = 0;

    ttl_time_lsa_clib_100 =
        Apply_Security_Loop((uint8_t* )raw_tc_sdls_ping_b, raw_tc_sdls_ping_len, ptr_enc_frame, &enc_frame_len, num_frames_100);

    printf("\nLSA+LIBG Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_100);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_lsa_clib_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_100)/ttl_time_lsa_clib_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_lsa_clib_100, "LSA+LIBG Apply Security MED", num_frames_100, 0);

    Crypto_Shutdown();
    free(raw_tc_sdls_ping_b);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, MDB_LIBG_MED_100)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_MARIADB, CRYPTOGRAPHY_TYPE_LIBGCRYPT, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_short = "202C05E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b00313011";
    
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_short, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_100);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_100);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_100)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "MDB+LIBG Apply Security MED", num_frames_100, 0);
    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, LSA_KMC_MED_100)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_KMCCRYPTO, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_Kmc_Crypto_Service("https", "client-demo-kmc.example.com", 8443, "crypto-service","/home/itc/Desktop/CERTS/ammos-ca-bundle.crt",NULL, CRYPTO_FALSE, "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "PEM","/home/itc/Desktop/CERTS/ammos-client-key.pem", NULL, NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_short = "202C05E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b00313011"; 
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_short, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);

    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_100);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_100);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_100)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "LSA+KMC Apply Security MED", num_frames_100, 0);
    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, MDB_KMC_MED_100)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_MARIADB, CRYPTOGRAPHY_TYPE_KMCCRYPTO, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Kmc_Crypto_Service("https", "client-demo-kmc.example.com", 8443, "crypto-service","/home/itc/Desktop/CERTS/ammos-ca-bundle.crt",NULL, CRYPTO_FALSE, "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "PEM","/home/itc/Desktop/CERTS/ammos-client-key.pem", NULL, NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_long = "202C05E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b00313011";
    
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_long, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_100);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_100);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_100)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");

    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "MDB+KMC Apply Security MED", num_frames_100, 0);

    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, LSA_LIBG_LONG_100)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init_Unit_Test();
    char* raw_tc_sdls_ping_h = "202C07E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b0031626E";
    char* raw_tc_sdls_ping_b = NULL;
    int raw_tc_sdls_ping_len = 0;
    SadbRoutine sadb_routine = get_sadb_routine_inmemory();

    float ttl_time_lsa_clib_100 = 0.0;

    hex_conversion(raw_tc_sdls_ping_h, &raw_tc_sdls_ping_b, &raw_tc_sdls_ping_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    SecurityAssociation_t* test_association = malloc(sizeof(SecurityAssociation_t) * sizeof(uint8_t));
    // Expose the SADB Security Association for test edits.
    sadb_routine->sadb_get_sa_from_spi(1, &test_association);
    test_association->sa_state = SA_NONE;
    sadb_routine->sadb_get_sa_from_spi(4, &test_association);
    test_association->sa_state = SA_OPERATIONAL;
    test_association->arsn_len = 0;

    ttl_time_lsa_clib_100 =
        Apply_Security_Loop((uint8_t* )raw_tc_sdls_ping_b, raw_tc_sdls_ping_len, ptr_enc_frame, &enc_frame_len, num_frames_100);

    printf("\nLSA+LIBG Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_100);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_lsa_clib_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_100)/ttl_time_lsa_clib_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_lsa_clib_100, "LSA+LIBG Apply Security LONG", num_frames_100, 0);

    Crypto_Shutdown();
    free(raw_tc_sdls_ping_b);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, MDB_LIBG_LONG_100)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_MARIADB, CRYPTOGRAPHY_TYPE_LIBGCRYPT, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_long = "202C07E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b0031626E";
    
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_long, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);

    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_100);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_100);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_100)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "MDB+LIBG Apply Security LONG", num_frames_100, 0);

    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, LSA_KMC_LONG_100)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_KMCCRYPTO, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Kmc_Crypto_Service("https", "client-demo-kmc.example.com", 8443, "crypto-service","/home/itc/Desktop/CERTS/ammos-ca-bundle.crt",NULL, CRYPTO_FALSE, "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "PEM","/home/itc/Desktop/CERTS/ammos-client-key.pem", NULL, NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_long = "202C07E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b0031626E";
    
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_long, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_100);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_100);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_100)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "LSA+KMC Apply Security LONG", num_frames_100, 0);
    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, MDB_KMC_LONG_100)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_MARIADB, CRYPTOGRAPHY_TYPE_KMCCRYPTO, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Kmc_Crypto_Service("https", "client-demo-kmc.example.com", 8443, "crypto-service","/home/itc/Desktop/CERTS/ammos-ca-bundle.crt",NULL, CRYPTO_FALSE, "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "PEM","/home/itc/Desktop/CERTS/ammos-client-key.pem", NULL, NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_long = "202C07E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b0031626E";
    
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_long, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_100);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_100);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_100)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");

    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "MDB+KMC Apply Security LONG", num_frames_100, 0);

    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, LSA_LIBG_SHORT_1000)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init_Unit_Test();
    char* raw_tc_sdls_ping_h = "202C046100ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEFF01C";
                                
    char* raw_tc_sdls_ping_b = NULL;
    int raw_tc_sdls_ping_len = 0;
    SadbRoutine sadb_routine = get_sadb_routine_inmemory();

    float ttl_time_lsa_clib_100 = 0.0;

    hex_conversion(raw_tc_sdls_ping_h, &raw_tc_sdls_ping_b, &raw_tc_sdls_ping_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    SecurityAssociation_t* test_association = malloc(sizeof(SecurityAssociation_t) * sizeof(uint8_t));
    // Expose the SADB Security Association for test edits.
    sadb_routine->sadb_get_sa_from_spi(1, &test_association);
    test_association->sa_state = SA_NONE;
    sadb_routine->sadb_get_sa_from_spi(10, &test_association);
    test_association->sa_state = SA_OPERATIONAL;
    test_association->arsn_len = 0;

    ttl_time_lsa_clib_100 =
        Apply_Security_Loop((uint8_t* )raw_tc_sdls_ping_b, raw_tc_sdls_ping_len, ptr_enc_frame, &enc_frame_len, num_frames_1K);

    printf("\nLSA+LIBG Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_1K);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_lsa_clib_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_1K)/ttl_time_lsa_clib_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_lsa_clib_100, "LSA+LIBG Apply Security SHORT", num_frames_1K, 0);

    Crypto_Shutdown();
    free(raw_tc_sdls_ping_b);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, MDB_LIBG_SHORT_1000)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_MARIADB, CRYPTOGRAPHY_TYPE_LIBGCRYPT, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_short = "202C046100ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEFF01C";
    
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_short, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_1K);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_1K);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_1K)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "MDB+LIBG Apply Security SHORT", num_frames_1K, 0);
    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, LSA_KMC_SHORT_1000)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_KMCCRYPTO, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_Kmc_Crypto_Service("https", "client-demo-kmc.example.com", 8443, "crypto-service","/home/itc/Desktop/CERTS/ammos-ca-bundle.crt",NULL, CRYPTO_FALSE, "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "PEM","/home/itc/Desktop/CERTS/ammos-client-key.pem", NULL, NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_short = "202C046100ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEFF01C"; 
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_short, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_1K);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_1K);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_1K)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "LSA+KMC Apply Security SHORT", num_frames_1K, 0);
    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, MDB_KMC_SHORT_1000)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_MARIADB, CRYPTOGRAPHY_TYPE_KMCCRYPTO, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Kmc_Crypto_Service("https", "client-demo-kmc.example.com", 8443, "crypto-service","/home/itc/Desktop/CERTS/ammos-ca-bundle.crt",NULL, CRYPTO_FALSE, "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "PEM","/home/itc/Desktop/CERTS/ammos-client-key.pem", NULL, NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    
    char* raw_tc_jpl_mmt_scid44_vcid1_short = "202C046100ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEFF01C";
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;

    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_short, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_1K);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_1K);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_1K)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "MDB+KMC Apply Security SHORT", num_frames_1K, 0);
    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, LSA_LIBG_MED_1000)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init_Unit_Test();
    char* raw_tc_sdls_ping_h = "202C05E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b00313011";
                                
    char* raw_tc_sdls_ping_b = NULL;
    int raw_tc_sdls_ping_len = 0;
    SadbRoutine sadb_routine = get_sadb_routine_inmemory();

    float ttl_time_lsa_clib_100 = 0.0;

    hex_conversion(raw_tc_sdls_ping_h, &raw_tc_sdls_ping_b, &raw_tc_sdls_ping_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    SecurityAssociation_t* test_association = malloc(sizeof(SecurityAssociation_t) * sizeof(uint8_t));
    // Expose the SADB Security Association for test edits.
    sadb_routine->sadb_get_sa_from_spi(1, &test_association);
    test_association->sa_state = SA_NONE;
    sadb_routine->sadb_get_sa_from_spi(10, &test_association);
    test_association->sa_state = SA_OPERATIONAL;
    test_association->arsn_len = 0;

    ttl_time_lsa_clib_100 =
        Apply_Security_Loop((uint8_t* )raw_tc_sdls_ping_b, raw_tc_sdls_ping_len, ptr_enc_frame, &enc_frame_len, num_frames_1K);

    printf("\nLSA+LIBG Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_1K);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_lsa_clib_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_1K)/ttl_time_lsa_clib_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_lsa_clib_100, "LSA+LIBG Apply Security MED", num_frames_1K, 0);

    Crypto_Shutdown();
    free(raw_tc_sdls_ping_b);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, MDB_LIBG_MED_1000)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_MARIADB, CRYPTOGRAPHY_TYPE_LIBGCRYPT, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_short = "202C05E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b00313011";
    
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_short, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_1K);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_1K);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_1K)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "MDB+LIBG Apply Security MED", num_frames_1K, 0);
    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, LSA_KMC_MED_1000)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_KMCCRYPTO, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_Kmc_Crypto_Service("https", "client-demo-kmc.example.com", 8443, "crypto-service","/home/itc/Desktop/CERTS/ammos-ca-bundle.crt",NULL, CRYPTO_FALSE, "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "PEM","/home/itc/Desktop/CERTS/ammos-client-key.pem", NULL, NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_short = "202C05E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b00313011"; 
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_short, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);

    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_1K);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_1K);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_1K)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "LSA+KMC Apply Security MED", num_frames_1K, 0);
    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, MDB_KMC_MED_1000)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_MARIADB, CRYPTOGRAPHY_TYPE_KMCCRYPTO, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Kmc_Crypto_Service("https", "client-demo-kmc.example.com", 8443, "crypto-service","/home/itc/Desktop/CERTS/ammos-ca-bundle.crt",NULL, CRYPTO_FALSE, "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "PEM","/home/itc/Desktop/CERTS/ammos-client-key.pem", NULL, NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_long = "202C05E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b00313011";
    
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_long, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_1K);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_1K);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_1K)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");

    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "MDB+KMC Apply Security MED", num_frames_1K, 0);

    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, LSA_LIBG_LONG_1000)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init_Unit_Test();
    char* raw_tc_sdls_ping_h = "202C07E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b0031626E";
    char* raw_tc_sdls_ping_b = NULL;
    int raw_tc_sdls_ping_len = 0;
    SadbRoutine sadb_routine = get_sadb_routine_inmemory();

    float ttl_time_lsa_clib_100 = 0.0;

    hex_conversion(raw_tc_sdls_ping_h, &raw_tc_sdls_ping_b, &raw_tc_sdls_ping_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    SecurityAssociation_t* test_association = malloc(sizeof(SecurityAssociation_t) * sizeof(uint8_t));
    // Expose the SADB Security Association for test edits.
    sadb_routine->sadb_get_sa_from_spi(1, &test_association);
    test_association->sa_state = SA_NONE;
    sadb_routine->sadb_get_sa_from_spi(4, &test_association);
    test_association->sa_state = SA_OPERATIONAL;
    test_association->arsn_len = 0;

    ttl_time_lsa_clib_100 =
        Apply_Security_Loop((uint8_t* )raw_tc_sdls_ping_b, raw_tc_sdls_ping_len, ptr_enc_frame, &enc_frame_len, num_frames_1K);

    printf("\nLSA+LIBG Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_1K);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_lsa_clib_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_1K)/ttl_time_lsa_clib_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_lsa_clib_100, "LSA+LIBG Apply Security LONG", num_frames_1K, 0);

    Crypto_Shutdown();
    free(raw_tc_sdls_ping_b);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, MDB_LIBG_LONG_1000)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_MARIADB, CRYPTOGRAPHY_TYPE_LIBGCRYPT, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_long = "202C07E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b0031626E";
    
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_long, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);

    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_1K);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_1K);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_1K)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "MDB+LIBG Apply Security LONG", num_frames_1K, 0);

    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, LSA_KMC_LONG_1000)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_INMEMORY, CRYPTOGRAPHY_TYPE_KMCCRYPTO, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Kmc_Crypto_Service("https", "client-demo-kmc.example.com", 8443, "crypto-service","/home/itc/Desktop/CERTS/ammos-ca-bundle.crt",NULL, CRYPTO_FALSE, "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "PEM","/home/itc/Desktop/CERTS/ammos-client-key.pem", NULL, NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_long = "202C07E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b0031626E";
    
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_long, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_1K);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_1K);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_1K)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");
    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "LSA+KMC Apply Security LONG", num_frames_1K, 0);
    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);
    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST(PERFORMANCE, MDB_KMC_LONG_1000)
{
    // Setup & Initialize CryptoLib
    Crypto_Config_CryptoLib(SADB_TYPE_MARIADB, CRYPTOGRAPHY_TYPE_KMCCRYPTO, CRYPTO_TC_CREATE_FECF_TRUE, TC_PROCESS_SDLS_PDUS_FALSE, TC_NO_PUS_HDR,
                            TC_IGNORE_SA_STATE_FALSE, TC_IGNORE_ANTI_REPLAY_TRUE, TC_UNIQUE_SA_PER_MAP_ID_FALSE,
                            TC_CHECK_FECF_TRUE, 0x3F, SA_INCREMENT_NONTRANSMITTED_IV_TRUE);
    Crypto_Config_MariaDB("client-demo-kmc.example.com","sadb", 3306,CRYPTO_TRUE,CRYPTO_TRUE, "/home/itc/Desktop/CERTS/ammos-ca-bundle.crt", NULL,  "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "/home/itc/Desktop/CERTS/ammos-client-key.pem",NULL,"robert", NULL);
    Crypto_Config_Kmc_Crypto_Service("https", "client-demo-kmc.example.com", 8443, "crypto-service","/home/itc/Desktop/CERTS/ammos-ca-bundle.crt",NULL, CRYPTO_FALSE, "/home/itc/Desktop/CERTS/ammos-client-cert.pem", "PEM","/home/itc/Desktop/CERTS/ammos-client-key.pem", NULL, NULL);
    Crypto_Config_Add_Gvcid_Managed_Parameter(0, 0x002C, 1, TC_HAS_FECF, TC_NO_SEGMENT_HDRS, 1024);
    int32_t status = Crypto_Init();
    char* raw_tc_jpl_mmt_scid44_vcid1_long = "202C07E1000080d2c70008197fABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234567890b0031626E";
    
    char* raw_tc_jpl_mmt_scid44_vcid1_expect = NULL;
    int raw_tc_jpl_mmt_scid44_vcid1_expect_len = 0;
    float ttl_time_mdb_kmc_100 = 0.0;
    
    hex_conversion(raw_tc_jpl_mmt_scid44_vcid1_long, &raw_tc_jpl_mmt_scid44_vcid1_expect, &raw_tc_jpl_mmt_scid44_vcid1_expect_len);

    uint8_t* ptr_enc_frame = NULL;
    uint16_t enc_frame_len = 0;

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);


    ttl_time_mdb_kmc_100 = Apply_Security_Loop((uint8_t* )raw_tc_jpl_mmt_scid44_vcid1_expect, raw_tc_jpl_mmt_scid44_vcid1_expect_len, ptr_enc_frame, &enc_frame_len, num_frames_1K);
    
    printf("\nMDB+KMC Apply Security\n");
    printf("\tNumber of Frames Sent: %d\n", num_frames_1K);
    printf("\t\tEncrypted Bytes Per Frame: %d\n", enc_frame_len);
    printf("\t\tTotal Time: %f\n", ttl_time_mdb_kmc_100);
    printf("\tMbps: %f\n", (((enc_frame_len * 8 * num_frames_1K)/ttl_time_mdb_kmc_100)/1024/1024));
    printf("\n");

    Write_To_File(enc_frame_len, ttl_time_mdb_kmc_100, "MDB+KMC Apply Security LONG", num_frames_1K, 0);

    Crypto_Shutdown();
    free(raw_tc_jpl_mmt_scid44_vcid1_expect);
    free(ptr_enc_frame);

    ASSERT_EQ(CRYPTO_LIB_SUCCESS, status);
}

UTEST_MAIN();