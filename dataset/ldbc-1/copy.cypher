COPY Comment FROM 'dataset/ldbc-1/csv/comment_0_0.csv' (HEADER=true);
COPY Forum FROM 'dataset/ldbc-1/csv/forum_0_0.csv' (HEADER=true);
COPY Organisation FROM 'dataset/ldbc-1/csv/organisation_0_0.csv' (HEADER=true);
COPY Person FROM 'dataset/ldbc-1/csv/person_0_0.csv' (HEADER=true);
COPY Place FROM 'dataset/ldbc-1/csv/place_0_0.csv' (HEADER=true);
COPY Post FROM 'dataset/ldbc-1/csv/post_0_0.csv' (HEADER=true);
COPY Tag FROM 'dataset/ldbc-1/csv/tag_0_0.csv' (HEADER=true);
COPY Tagclass FROM 'dataset/ldbc-1/csv/tagclass_0_0.csv' (HEADER=true);
COPY containerOf FROM 'dataset/ldbc-1/csv/forum_containerOf_post_0_0.csv' (HEADER=true);
COPY commentHasCreator FROM 'dataset/ldbc-1/csv/comment_hasCreator_person_0_0.csv' (HEADER=true);
COPY postHasCreator FROM 'dataset/ldbc-1/csv/post_hasCreator_person_0_0.csv' (HEADER=true);
COPY hasInterest FROM 'dataset/ldbc-1/csv/person_hasInterest_tag_0_0.csv' (HEADER=true);
COPY hasMember FROM 'dataset/ldbc-1/csv/forum_hasMember_person_0_0.csv' (HEADER=true);
COPY hasModerator FROM 'dataset/ldbc-1/csv/forum_hasModerator_person_0_0.csv' (HEADER=true);
COPY commentHasTag FROM 'dataset/ldbc-1/csv/comment_hasTag_tag_0_0.csv' (HEADER=true);
COPY forumHasTag FROM 'dataset/ldbc-1/csv/forum_hasTag_tag_0_0.csv' (HEADER=true);
COPY postHasTag FROM 'dataset/ldbc-1/csv/post_hasTag_tag_0_0.csv' (HEADER=true);
COPY hasType FROM 'dataset/ldbc-1/csv/tag_hasType_tagclass_0_0.csv' (HEADER=true);
COPY commentIsLocatedIn FROM 'dataset/ldbc-1/csv/comment_isLocatedIn_place_0_0.csv' (HEADER=true);
COPY organisationIsLocatedIn FROM 'dataset/ldbc-1/csv/organisation_isLocatedIn_place_0_0.csv' (HEADER=true);
COPY personIsLocatedIn FROM 'dataset/ldbc-1/csv/person_isLocatedIn_place_0_0.csv' (HEADER=true);
COPY postIsLocatedIn FROM 'dataset/ldbc-1/csv/post_isLocatedIn_place_0_0.csv' (HEADER=true);
COPY isPartOf FROM 'dataset/ldbc-1/csv/place_isPartOf_place_0_0.csv' (HEADER=true);
COPY isSubclassOf FROM 'dataset/ldbc-1/csv/tagclass_isSubclassOf_tagclass_0_0.csv' (HEADER=true);
COPY knows FROM 'dataset/ldbc-1/csv/person_knows_person_0_0.csv' (HEADER=true);
COPY likeComment FROM 'dataset/ldbc-1/csv/person_likes_comment_0_0.csv' (HEADER=true);
COPY likePost FROM 'dataset/ldbc-1/csv/person_likes_post_0_0.csv' (HEADER=true);
COPY replyOfComment FROM 'dataset/ldbc-1/csv/comment_replyOf_comment_0_0.csv' (HEADER=true);
COPY replyOfPost FROM 'dataset/ldbc-1/csv/comment_replyOf_post_0_0.csv' (HEADER=true);
COPY studyAt FROM 'dataset/ldbc-1/csv/person_studyAt_organisation_0_0.csv' (HEADER=true);
COPY workAt FROM 'dataset/ldbc-1/csv/person_workAt_organisation_0_0.csv' (HEADER=true);
