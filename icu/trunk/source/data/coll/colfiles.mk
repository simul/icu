# *   Copyright (C) 1998-2005, International Business Machines
# *   Corporation and others.  All Rights Reserved.
# A list of txt's to build
# Note: 
#
#   If you are thinking of modifying this file, READ THIS. 
#
# Instead of changing this file [unless you want to check it back in],
# you should consider creating a 'collocal.mk' file in this same directory.
# Then, you can have your local changes remain even if you upgrade or
# reconfigure ICU.
#
# Example 'collocal.mk' files:
#
#  * To add an additional locale to the list: 
#    _____________________________________________________
#    |  COLLATION_SOURCE_LOCAL =   myLocale.txt ...
#
#  * To REPLACE the default list and only build with a few
#     locale:
#    _____________________________________________________
#    |  COLLATION_SOURCE = ar.txt ar_AE.txt en.txt de.txt zh.txt
#
#
# Generated by LDML2ICUConverter, from LDML source files. 

# Aliases which do not have a corresponding xx.xml file (see deprecatedList.xml)
COLLATION_SYNTHETIC_ALIAS = de__PHONEBOOK.txt es__TRADITIONAL.txt hi__DIRECT.txt zh_TW_STROKE.txt\
 zh__PINYIN.txt


# All aliases (to not be included under 'installed'), but not including root.
COLLATION_ALIAS_SOURCE = $(COLLATION_SYNTHETIC_ALIAS)


# Empty locales, used for validSubLocale fallback.
COLLATION_EMPTY_SOURCE = ar_AE.txt ar_BH.txt ar_DZ.txt ar_EG.txt\
 ar_IN.txt ar_IQ.txt ar_JO.txt ar_KW.txt ar_LB.txt\
 ar_LY.txt ar_MA.txt ar_OM.txt ar_QA.txt ar_SA.txt\
 ar_SD.txt ar_SY.txt ar_TN.txt ar_YE.txt be_BY.txt\
 bg_BG.txt ca_ES.txt cs_CZ.txt da_DK.txt de_AT.txt\
 de_BE.txt de_CH.txt de_DE.txt de_LU.txt el_GR.txt\
 en_AU.txt en_BW.txt en_CA.txt en_GB.txt en_HK.txt\
 en_IE.txt en_IN.txt en_MT.txt en_NZ.txt en_PH.txt\
 en_SG.txt en_US.txt en_US_POSIX.txt en_VI.txt en_ZA.txt\
 en_ZW.txt es_AR.txt es_BO.txt es_CL.txt es_CO.txt\
 es_CR.txt es_DO.txt es_EC.txt es_ES.txt es_GT.txt\
 es_HN.txt es_MX.txt es_NI.txt es_PA.txt es_PE.txt\
 es_PR.txt es_PY.txt es_SV.txt es_US.txt es_UY.txt\
 es_VE.txt et_EE.txt fa_IR.txt fi_FI.txt fo_FO.txt\
 fr_BE.txt fr_CA.txt fr_CH.txt fr_FR.txt fr_LU.txt\
 ga.txt ga_IE.txt gu_IN.txt he_IL.txt hi_IN.txt\
 hr_HR.txt hu_HU.txt id.txt id_ID.txt is_IS.txt\
 it_CH.txt it_IT.txt ja_JP.txt kk_KZ.txt kl_GL.txt\
 kn_IN.txt ko_KR.txt lt_LT.txt lv_LV.txt mk_MK.txt\
 mr_IN.txt ms.txt ms_BN.txt ms_MY.txt mt_MT.txt\
 nb_NO.txt nl.txt nl_BE.txt nl_NL.txt nn_NO.txt\
 om_ET.txt om_KE.txt pa_IN.txt pl_PL.txt ps_AF.txt\
 pt.txt pt_BR.txt pt_PT.txt ro_RO.txt ru_RU.txt\
 ru_UA.txt sh_YU.txt sk_SK.txt sl_SI.txt sq_AL.txt\
 sr_YU.txt sv_FI.txt sv_SE.txt ta_IN.txt te_IN.txt\
 th_TH.txt tr_TR.txt uk_UA.txt vi_VN.txt zh_CN.txt\
 zh_SG.txt


# Ordinary resources
COLLATION_SOURCE = $(COLLATION_EMPTY_SOURCE) ar.txt be.txt bg.txt ca.txt\
 cs.txt da.txt de.txt el.txt en.txt\
 en_BE.txt eo.txt es.txt et.txt fa.txt\
 fa_AF.txt fi.txt fo.txt fr.txt gu.txt\
 he.txt hi.txt hr.txt hu.txt is.txt\
 it.txt ja.txt kk.txt kl.txt kn.txt\
 ko.txt lt.txt lv.txt mk.txt ml.txt\
 mr.txt mt.txt nb.txt nn.txt om.txt\
 or.txt pa.txt pl.txt ps.txt ro.txt\
 ru.txt sh.txt sk.txt sl.txt sq.txt\
 sr.txt sv.txt ta.txt te.txt th.txt\
 tr.txt uk.txt vi.txt zh.txt zh_HK.txt\
 zh_Hant.txt zh_MO.txt zh_TW.txt

