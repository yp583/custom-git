import json
from uuid import UUID

from rest_framework.response import Response
from django.http import JsonResponse, HttpResponse
from rest_framework.views import APIView
from rest_framework import status
from openai import OpenAI
from datetime import datetime
from django.utils import timezone
from django.http import Http404
import jwt

from .altrix.capibara import CAPIbara
from .altrix.epicapi import eapi
from altrix_backend.models import Field, AudioRecording, Transcript, Interaction, Form, User, FormTemplate, FieldTemplate, InteractionStatus, TranscriptStatus, Embedding, ExtractionJob, JobStatus, Patient, ChatMessage, ChatHistory, SenderType, Chart
from .altrix.utils import get_model, get_sha256_hash, split_text
from .altrix.extraction import extract, extract_from_audio_recording
from altrix_backend import assistant_agent
from altrix_backend.altrix.ehr_assistant.graph import PatientInfo, PractionerInfo

import requests as r

from .serializers import AudioRecordingSerializer, TranscriptSerializer, \
    DisplayInteractionSerializer, FormSearchSerializer, FieldSearchSerializer, EmbeddingSearchSerializer, \
        DetailedInteractionSerializer, FormSearchSerializer, FieldSearchSerializer, PatientSerializer, InteractionDetailsSerializer, ChatMessageSerializer, ChatHistorySerializer

from langchain_core.messages import AIMessage, HumanMessage
from altrix_backend.altrix.fhir_api import search_patient, get_patient_info

capi = CAPIbara("3636c4b0-854e-41b7-8119-ea87e976248d", "")

#testing diff

class Auth(APIView):
    def get(self, request):
        client_id = 'aaad030f-02e5-42f8-abcb-08784cdcbb3c'
        url = f"https://fhir.epic.com/interconnect-fhir-oauth/oauth2/authorize"
        headers = {
            "Content-Type": "application/x-www-form-urlencoded"
        }
        data = {
            "response_type": "code",
            "scope": "launch launch/provider offline_access",
            "client_id": client_id,
            "launch": request.GET['launch'],
            "aud": request.GET['iss'],
            "redirect_uri": "http://localhost:8000/auth_callback/",
            "state": "98wrghuwuogerg97",

        }
        response = r.post(url, headers=headers, data=data).json()
        epapi = CAPIbara(
            client_id="aaad030f-02e5-42f8-abcb-08784cdcbb3c",
            base_url="https://fhir.epic.com/interconnect-fhir-oauth/api/FHIR/R4"
        )
        epapi.discover_urls()

        headers = {
                "Content-Type": "application/x-www-form-urlencoded",
        }
        data = {
            "grant_type": "authorization_code",
            "code": response['code'],
            "client_id": client_id,
            "redirect_uri": "http://localhost:8000/auth_callback/",
        }
        response = r.post(epapi.token_url, headers=headers, data=data).json()
        return JsonResponse(response)
    

class AuthCallback(APIView):
    def get(self, request):
        return JsonResponse(request.GET)


class Login(APIView):
    def post(self, request):
        try:
            print(request.user_object)
            return JsonResponse({'hashed_id': request.user_object.user_id}, status=status.HTTP_200_OK)
        except Exception as e:
            return JsonResponse({'error': f'Invalid Login: {str(e)}'}, status=403)
            
        # eapi.discover_urls()
        # try:
        #     print("1111here")
        #     token_obj = eapi.get_token(state="", code=request.data.get('code'))
        #     print("not here")
        #     access_token = token_obj['access_token']
        #     refresh_token = token_obj['refresh_token']
        #     id_token = token_obj['id_token']
        #     decoded_id_token = jwt.decode(id_token, options={"verify_signature": False})
        #     hashed_provider_id = get_sha256_hash(decoded_id_token['fhirUser'].split('/')[-1])
        #     try:
        #         User.objects.get(User, user_id=hashed_provider_id) #get user from db
        #     except:
        #         new_user = User(                                   #make new user in db
        #             user_id=hashed_provider_id,
        #             refresh_token=refresh_token,
        #             last_token_requested_timestamp=timezone.now(),
        #             curr_access_token=access_token,
        #         )
        #         new_user.save()
            
        #     #request.session['hashed_provider_id'] = hashed_provider_id
        #     return JsonResponse({"data": hashed_provider_id, "details": "successfully authenticated user in new session"})
            
        
class Audio(APIView):
    def post(self, request):
        if "audio" not in request.FILES:
            return Response({"data": "error", "details": "no audio file found in request"}, status.HTTP_400_BAD_REQUEST)
        if "interaction_id" not in request.data:
            return Response({"data": "error", "details": "no interaction id found in request"}, status.HTTP_400_BAD_REQUEST)

        try:
            file = request.FILES['audio']
            interaction = get_model(Interaction, uuid=request.data['interaction_id'])
            audio = AudioRecording(interaction=interaction)
            audio.audio_file.save(str(audio.uuid) + ".mp3", file)
            audio.save()
            audio.sync_gen_transcript()
            extract_from_audio_recording(audio)
        except Exception as e:
            print("ERROR: ", e)
            return Response({"data": "error", "details": str(e)}, status=status.HTTP_500_INTERNAL_SERVER_ERROR)

        return Response({"data": audio.uuid, "details": "successfully uploaded audio file and returned id"},
                        status=status.HTTP_200_OK)

    def get(self, request, audio_id):
        if audio_id is None:
            return Response({"data": "error", "details": "no id found in request"}, status.HTTP_400_BAD_REQUEST)

        try:
            audio = get_model(AudioRecording, uuid=audio_id)
            audio_serializer = AudioRecordingSerializer(audio)
            response_data = {
                'data': audio_serializer.data,
                "details": "returned audio file url"
            }

            return Response(response_data, status=status.HTTP_200_OK)
        except Exception as e:
            return Response({"data": "error", "details": str(e)}, status=status.HTTP_400_BAD_REQUEST)


class TranscriptRoute(APIView):
    def post(self, request):
        if 'audio_id' not in request.data:
            return Response({"data": "error", "details": "no audio id found in request"}, status.HTTP_400_BAD_REQUEST)

        uuid = request.data['audio_id']
        audio = get_model(AudioRecording, uuid=uuid)

        try:
            audio.gen_transcript()
            return Response(
                {"data": "success", "details": "successfully created transcript from audio and returned id"},
                status=status.HTTP_200_OK)

        except Exception as e:
            return Response({"data": "error", "details": str(e)}, status=status.HTTP_500_INTERNAL_SERVER_ERROR)

    def get(self, request, transcript_id):
        if transcript_id is None:
            return Response({"data": "error", "details": "no id found in request"}, status.HTTP_400_BAD_REQUEST)

        try:
            transcript = get_model(Transcript, uuid=transcript_id)
            transcript_serializer = TranscriptSerializer(transcript)
            response_data = {
                'data': transcript_serializer.data,
                'details': 'returned transcript'
            }
            return Response(response_data, status=status.HTTP_200_OK)
        except Exception as e:
            return Response({"data": str(e), "details": "no transcript found with id given"},
                            status=status.HTTP_400_BAD_REQUEST)


# Expects JSON data 
class InteractionRoute(APIView):
    def post(self, request):
        req_vars = ['patient_ehr_id', 'form_loincs', 'field_loincs', 'tags']
        print("REQUEST DATA: ", request.data)
        for req_var in req_vars:
            if req_var not in request.data:
                return Response({"data": "error", "details": f"no {req_var} found in request"},
                                status.HTTP_400_BAD_REQUEST)
            
        try:
            tags = json.loads(request.data.get('tags', '[]'))
        except json.JSONDecodeError:
            print("Error: 'tags' is not valid JSON")

        interaction = Interaction()
        interaction.patient_ehr_id = request.data['patient_ehr_id']
        interaction.tags = tags
        interaction.user = request.user_object
        interaction.save()

        print("Saving interaction: ", interaction.uuid)

        try:
            form_loincs = json.loads(request.data.get('form_loincs', '[]'))
            for loinc_code in form_loincs:
                template = get_model(FormTemplate, loinc_code=loinc_code)
                form = Form(template=template, interaction=interaction)
                form.save()
                form.gen_fields_from_template()
                form.save()
                print("Saving form: ", form.uuid)
        except Exception as e:
            interaction.delete()
            return Response({"data": e, "details": str(e)},
                            status=status.HTTP_500_INTERNAL_SERVER_ERROR)
    
        try:
            field_loincs = json.loads(request.data.get('field_loincs', '[]'))
            for field_code in field_loincs:
                template = get_model(FieldTemplate, loinc_code=field_code)
                field = Field(template=template, interaction=interaction)
                field.save()
                print("Saving field: ", field.uuid)
        except Exception as e:
            interaction.delete()
            return Response({"data": e, "details": str(e)},
                            status=status.HTTP_500_INTERNAL_SERVER_ERROR)

        interaction.save()
        return Response({"data": interaction.uuid, "details": "successfully created interaction"},
                        status=status.HTTP_200_OK)

    def get(self, request, interaction_id):
        if interaction_id is None:
            return Response({"data": "error", "details": "no id found in request"}, status.HTTP_400_BAD_REQUEST)

        try:
            interaction = get_model(Interaction, uuid=interaction_id)
            interaction_serializer = DetailedInteractionSerializer(interaction)

            response_data = {
                "data": interaction_serializer.data,
                "details": "returned transcript"
            }

            return Response(response_data, status=status.HTTP_200_OK)
        except Exception as e:
            return Response({"data": str(e), "details": "no interaction found with id given"},
                            status=status.HTTP_400_BAD_REQUEST)
        

class Extract(APIView):
    def post(self, request):
        req_vars = ['interaction_id']
        for req_var in req_vars:
            if req_var not in request.data:
                return Response({"data": "error", "details": f"no {req_var} found in request"},
                                status.HTTP_400_BAD_REQUEST)
        interaction = get_model(Interaction, uuid=request.data['interaction_id'])

        if interaction.status != InteractionStatus.VALIDATING.value:
            try:
                job = ExtractionJob(interaction = interaction, user=request.user_object)
                job.save()
                extract(interaction, job)
            except Exception as e: 
                job.status = JobStatus.ERROR.value
                job.save(update_fields=['status'])
                return Response({"data": {}, "details": f"failed on extraction {str(e)}"}, status=status.HTTP_500_INTERNAL_SERVER_ERROR)
            finally:
                job.delete()
        return Response({"data": job.uuid, "details": "successfully returned extracted info"}, status=status.HTTP_200_OK)

    def get(self, request, interaction_id):
        if interaction_id is None:
            return Response({"data": "error", "details": "no id found in request"}, status.HTTP_400_BAD_REQUEST)

        try:
            interaction = get_model(Interaction, uuid=interaction_id)
            if interaction.status != InteractionStatus.VALIDATING.value:
                return Response({"data": "error", "details": "interaction not staged for validating"}, status.HTTP_400_BAD_REQUEST) 

            response_data = {
                "data": interaction.extracted_data,
                "details": "returned extracted data"
            }
            return Response(response_data, status=status.HTTP_200_OK)
        except Exception as e:
            return Response({"data": "error", "details": str(e)},
                            status=status.HTTP_500_INTERNAL_SERVER_ERROR)


class UserRoute(APIView):
    def get(self, request, route):
        if route == 'current_interactions':
            return self.get_current_interactions(request)
        else:
            return Response({"message": "Invalid route"}, status=400)


    @staticmethod
    def get_interactions_by_status(user, i_status):
        queried_interactions = user.interactions.filter(status=i_status.value)
        interaction_serializer = DisplayInteractionSerializer(queried_interactions, many=True)
        return interaction_serializer.data

    def get_current_interactions(self, request):
        response_data = {
            "data": {
                'queued': self.get_interactions_by_status(request.user_object, InteractionStatus.QUEUED),
                'validating': self.get_interactions_by_status(request.user_object, InteractionStatus.VALIDATING),
            },
            "details": f"returned current interactions"
        }
        return Response(response_data,
                        status=status.HTTP_200_OK)


from rest_framework.parsers import JSONParser, MultiPartParser

def get_search_results(label_filter, requested_types):

    results = {}

    class_types = {
        "flowsheets": (FormTemplate, FormSearchSerializer),
        "fields": (FieldTemplate, FieldSearchSerializer)
    }

    for type_key in requested_types:
        search_function, serializer = class_types[type_key]
        search_function = search_function.objects.all() if not label_filter else search_function.objects.filter(label__search=label_filter)
        results[type_key] = serializer(search_function, many=True).data

    return results
# Get all forms and fields 
# label
class SearchFormsAndFields(APIView):
    # Search for forms and fields by label
    # Types must be a list of strings containing only 'forms' or 'fields'
    # label_filter is an optional the string to search for in the label, if it sempty then you dont apply a filter
    def post(self, request):

        if "types" not in request.data:
            error_message = f"missing keys: types"
            return Response({"data": "error", "details": error_message}, status=status.HTTP_400_BAD_REQUEST)
        
        valid_types = {'flowsheets', 'fields'}
        label_filter = request.data.get('query', '')

        types = request.data.get('types')
        print("content type",request.content_type)
        print(types, type(types))

        if not isinstance(types, list):
            return Response({"data": "error", "details": "types must be an array"}, status=status.HTTP_400_BAD_REQUEST)
        
        if not all(t in valid_types for t in types):
            return Response(
                {"data": "error", "details": f"types must only contain {valid_types}. It contains {types}"}, 
                status=status.HTTP_400_BAD_REQUEST
            )
        
        results = get_search_results(label_filter, types)

        return Response({'templates': results, "details": "returned search results of query"}, status=status.HTTP_200_OK)
    
    # def post(self, request):
    #     req_vars = ['query', 'types']
    #     for req_var in req_vars:
    #         if req_var not in request.data:
    #             return Response({"data": "error", "details": f"no {req_var} found in request"},
    #                             status.HTTP_400_BAD_REQUEST)
            
    #     types = json.loads(request.data['types'])

    #     results = [[], [], [], []]
    #     if request.data['query'] == "":
    #         Response({'data': results, "details": "returned search results of query"}, status=status.HTTP_200_OK)

    #     if 0 in types:
    #         types += [1, 2, 3]
    #     if 1 in types:
    #         form_results = FormTemplate.objects.filter(label__search=request.data['query'])
    #         form_results_data = FormSearchSerializer(form_results, many=True)
    #         results[1] = form_results_data.data
    #     if 3 in types:
    #         field_results = FieldTemplate.objects.filter(label__search=request.data['query'])
    #         field_results_data = FieldSearchSerializer(field_results, many=True)
    #         results[3] = field_results_data.data

    #     return Response({'data': results, "details": "returned search results of query"}, status=status.HTTP_200_OK)
    
    def get(self, request):
        try:
            # Retrieve all form templates
            form_templates = FormTemplate.objects.all()
            form_templates_data = FormSearchSerializer(form_templates, many=True).data

            # Retrieve all field templates
            field_templates = FieldTemplate.objects.all()
            field_templates_data = FieldSearchSerializer(field_templates, many=True).data

            # Prepare the response data
            response_data = {
                'form_templates': form_templates_data,
                'field_templates': field_templates_data,
                "details": "successfully retrieved all form and field templates"
            }

            return Response({'data': response_data}, status=status.HTTP_200_OK)
        except Exception as e:
            return Response({"data": "error", "details": str(e)}, status=status.HTTP_500_INTERNAL_SERVER_ERROR)


class EmbeddingRoute(APIView):
    def post(self, request):
        req_vars = ['interaction_id']
        for req_var in req_vars:
            if req_var not in request.data:
                return Response({"data": "error", "details": f"no {req_var} found in request"},
                                status.HTTP_400_BAD_REQUEST)

        interaction = get_model(Interaction, uuid=request.data['interaction_id'])

        total_transcripts = ""

        for audio_recording in interaction.audio_recordings.all():
            transcript = audio_recording.transcript
            if transcript.status == TranscriptStatus.FINISHED.value:
                total_transcripts += transcript.transcript + ". "

        documents = split_text(total_transcripts, ["\n\n", "\n", ". "], 0, 200)

        try:
            for document in documents:
                new_embedding = Embedding(interaction=interaction, document=document)
                new_embedding.save()
        except Exception as e:
            return Response({"data": "error", "details": f"trouble generating embeddings {str(e)}"},
                        status=status.HTTP_200_OK)

        return Response({"data": "success", "details": "successfully generated embeddings for interaction"},
                        status=status.HTTP_200_OK) 


    def get(self, request, interaction_id):
        if interaction_id is None:
            return Response({"data": "error", "details": "no id found in request"}, status.HTTP_400_BAD_REQUEST)
        if 'query' not in request.GET:
            return Response({"data": "error", "details": "no id found in request"}, status.HTTP_400_BAD_REQUEST)

        interaction = get_model(Interaction, uuid=interaction_id)

        results = interaction.embeddings.search(request.GET['query'])

        embedding_serializer = EmbeddingSearchSerializer(results, many=True)

        return Response({"data": embedding_serializer.data, "details": "successfully created interaction"},
                        status=status.HTTP_200_OK)


class EhrToken(APIView):
    def get(self, request):
        return Response({"data": request.ehr_token, "details": "successfully created interaction"},
                        status=status.HTTP_200_OK)



class PatientRoute(APIView):
    def post(self, request):
        print(request.data)
        if 'patient_id' not in request.data:
            return Response({"data": "error", "details": "no id found in request"}, status.HTTP_400_BAD_REQUEST)
        try:
            # Debug print to see the exact format
            # patient = get_model(Patient, uuid=request.data['patient_id']) #weird casting needed
            patient = get_patient_info(request.data['patient_id'])
            return Response(patient, status=status.HTTP_200_OK)
        except Exception as e:
            return Response({"data": str(e), "details": "no patient found with id given"},
                            status=status.HTTP_400_BAD_REQUEST)

class InteractionDisplay(APIView):
    def get(self, request):
        try:
            interactions = request.user_object.interactions.all()
            print(interactions)
            interaction_serializer = DisplayInteractionSerializer(interactions, many=True, context={'request': request})
            print(interaction_serializer.data)
            return Response(interaction_serializer.data, status=status.HTTP_200_OK)
        except Exception as e:
            return Response({"data": "error", "details": f"failed to retrieve interactions: {str(e)}"}, status=status.HTTP_400_BAD_REQUEST)
class HistoryInteractionDisplay(APIView):
    def get(self, request):
        try:
            interactions = request.user_object.interactions.filter(status=InteractionStatus.FINISHED.value)
            interaction_serializer = DisplayInteractionSerializer(interactions, many=True, context={'request': request})
            print(interaction_serializer.data)
            return Response(interaction_serializer.data, status=status.HTTP_200_OK)
        except Exception as e:
            return Response({"data": "error", "details": f"failed to retrieve history interactions: {str(e)}"}, status=status.HTTP_400_BAD_REQUEST)

class InteractionDetail(APIView):
    def get(self, request, interaction_id):
        interaction = get_model(Interaction, uuid=interaction_id)
        if interaction is Http404:
            return Response({"data": "error", "details": f"failed to retrieve interaction"}, status=status.HTTP_400_BAD_REQUEST)
        else:
            try:
                interaction_serializer = InteractionDetailsSerializer(interaction, context={'request': request})
                return Response(interaction_serializer.data, status=status.HTTP_200_OK)
            except Exception as e:
                return Response({"data": "error", "details": str(e)}, status=status.HTTP_400_BAD_REQUEST)

class searchForPatientRoute(APIView):
    def get(self, request):
        # Check required parameters
        required_params = ['first_name', 'last_name', 'dob']
        num_found = 0
        for param in required_params:
            if param in request.GET:
                num_found += 1
        if num_found == 0:
            return Response(
                {"data": "error", "details": f"Missing required parameters: {required_params}"}, 
                status=status.HTTP_400_BAD_REQUEST
            )
        
        try:
            # Parse the date string into a datetime object
            print(request.GET['first_name'], request.GET['last_name'], request.GET['dob'])
            # dob = datetime.strptime(request.GET['dob'], '%Y-%m-%d').date()
            
            # # Query the database for matching patients
            # patients = Patient.objects.filter(
            #     first_name=request.GET['first_name'],
            #     last_name=request.GET['last_name'],
            #     dob=dob
            # )
            
            #query the FHIR API for matching patients
            patients = search_patient(first_name=request.GET['first_name'], last_name=request.GET['last_name'], dob=request.GET['dob'])
            
            if len(patients) == 0:
                full_name = f"{request.GET['first_name']} {request.GET['last_name']}"
                return Response(
                    {"data": "error", "details": f'No matching patients found for {full_name}, dob={request.GET["dob"]} '}, 
                    status=status.HTTP_404_NOT_FOUND
                )
            
            # Serialize the results
            # patient_serializer = PatientSerializer(patients, many=True)
            
            response_data = {
                "data": patients,
                "details": "Successfully found matching patients"
            }
            return Response(response_data, status=status.HTTP_200_OK)
            
        except ValueError:
            return Response(
                {"data": "error", "details": "Invalid date format. Use YYYY-MM-DD"}, 
                status=status.HTTP_400_BAD_REQUEST
            )
        except Exception as e:
            return Response(
                {"data": "error", "details": f"Error searching for patients: {str(e)}"}, 
                status=status.HTTP_500_INTERNAL_SERVER_ERROR
            )

class ChatRoute(APIView):
    def post(self, request, patient_id):
        try:
            if 'message' not in request.data:
                return Response(
                    {"data": "error", "details": "no message found in request"},
                    status=status.HTTP_400_BAD_REQUEST
                )
                
            chat_history, created= ChatHistory.objects.get_or_create(
                user=request.user_object,
                patient_id=patient_id,
            )
            
            ChatMessage.objects.create(
                content=request.data['message'],
                sender=SenderType.USER.value,
                chat_history=chat_history
            )
            
            patient_info = get_patient_info(patient_id)
            print("PATIENT INFO: ", patient_info)
            pydantic_patient = PatientInfo(
                id=patient_info['id'],
                first_name=patient_info['firstName'],
                last_name=patient_info['lastName'],
                dob=patient_info['birthDate'],
                gender=patient_info['gender'],
            )
            
            pydantic_provider = PractionerInfo(
                id=request.user_object.provider_fhir_id,
            )
            
            result = assistant_agent.invoke({"messages": [HumanMessage(content=request.data['message'])], "patient_info": pydantic_patient, "provider_info": pydantic_provider}, config={"configurable": {"thread_id": request.user_object.user_id + "_" + patient_id}})
            chart = None
            if result["chart_info"]['chart_type'] != "None":
                chart = Chart.objects.create(
                    chart_type=result["chart_info"]["chart_type"],
                    x_axis=result["chart_info"]['chart_params']['x_axis'],
                    y_axis=result["chart_info"]['chart_params']['y_axis'],
                    title=result["chart_info"]['chart_params']['title'],
                    subtitle=result["chart_info"]['chart_params']['subtitle'],
                    chart_data=result["chart_info"]['chart_data']
                )
            
            
            agent_response = ChatMessage.objects.create(
                content=result["messages"][-1].content,
                sender=SenderType.ASSISTANT.value,
                chat_history=chat_history,
                chart=chart
            )
            agent_response_serializer = ChatMessageSerializer(agent_response)
            return Response(agent_response_serializer.data, status=status.HTTP_200_OK)

        except Exception as e:
            print("ERROR: ", e)
            return Response({
                "data": "error",
                "details": f"Error processing message: {str(e)}"
            }, status=status.HTTP_500_INTERNAL_SERVER_ERROR)

    def delete(self, request, patient_id):
        try:
            chat_history = ChatHistory.objects.get(
                user=request.user_object,
                patient_id=patient_id,
            )
            chat_history.messages.all().delete()
            assistant_agent.update_state({"configurable": {"thread_id": request.user_object.user_id + "_" + patient_id}},
                {
                    "messages": [],
                    "patient_info": None,
                    "provider_info": None,
                    "chart_info": None
                }
            )
            return Response({
                "data": "success",
                "details": "Chat history erased successfully"
            }, status=status.HTTP_200_OK)
        except Exception as e:
            return Response({
                "data": "error",
                "details": f"Error erasing chat history: {str(e)}"
            }, status=status.HTTP_500_INTERNAL_SERVER_ERROR)

    def get(self, request, patient_id):
        try:
            chat_history, created = ChatHistory.objects.get_or_create(
                user=request.user_object,
                patient_id=patient_id,
            )
            chat_messages_serializer = ChatMessageSerializer(chat_history.messages.all(), many=True)
            return Response(chat_messages_serializer.data, status=status.HTTP_200_OK)
        except Exception as e:
            return Response({
                "data": "error", 
                "details": f"Error retrieving messages: {str(e)}"
            }, status=status.HTTP_500_INTERNAL_SERVER_ERROR)
                
class ChatHistoryRoute(APIView):
    def get(self, request):
        try:
            chat_histories = ChatHistory.objects.filter(
                user=request.user_object,
            )
            
            # Serialize the messages
            chat_messages_serializer = ChatHistorySerializer(chat_histories, many=True)
            print("CHAT MESSAGES SERIALIZER: ", chat_messages_serializer.data)
            return Response(chat_messages_serializer.data, status=status.HTTP_200_OK)
            
        except Exception as e:
            return Response({
                "data": "error", 
                "details": f"Error retrieving messages: {str(e)}"
            }, status=status.HTTP_500_INTERNAL_SERVER_ERROR)
        
class ValidateInteraction(APIView):
    def post(self, request):
        try:
            interaction = get_model(Interaction, uuid=request.data['interaction_id'])
            interaction.status = InteractionStatus.FINISHED.value
            interaction.save(update_fields=['status'])
            return Response("successfully validated interaction", status=status.HTTP_200_OK)
        except Interaction.DoesNotExist:
            return Response(f"Interaction with id {request.data['interaction_id']} not found", status=status.HTTP_404_NOT_FOUND)
        except Exception as e:
            return Response(f"Error validating interaction: {str(e)}", status=status.HTTP_500_INTERNAL_SERVER_ERROR)

class UpdateInteraction(APIView):
    def post(self, request):
        if 'interaction_id' not in request.data:
            return Response({"data": "error", "details": "no interaction_id found in request"}, status=status.HTTP_400_BAD_REQUEST)

        try:
            interaction = get_model(Interaction, uuid=request.data['interaction_id'])

            interaction.tags = []

            print(request.data)
            if 'fields' in request.data:
                updated_fields = json.loads(request.data['fields'])
                
                if (len(updated_fields) != 0): 
                    interaction.tags.append("fields")

                interaction.fields.all().delete()
                for field_code in updated_fields:
                    field_template = get_model(FieldTemplate, loinc_code=field_code)
                    field = Field(template=field_template, interaction=interaction)
                    field.save()

            if 'flowsheets' in request.data:
                updated_flowsheets = json.loads(request.data['flowsheets'])

                if (len(updated_flowsheets) != 0): 
                    interaction.tags.append("flowsheets")

                interaction.forms.all().delete()
                for form_code in updated_flowsheets:
                    form_template = get_model(FormTemplate, loinc_code=form_code)
                    form = Form(template=form_template, interaction=interaction)
                    form.save()
                    form.gen_fields_from_template()

            interaction.save()

            return Response({"data": interaction.uuid, "details": "successfully updated interaction"}, status=status.HTTP_200_OK)

        except Interaction.DoesNotExist:
            return Response({"data": "error", "details": "interaction not found"}, status=status.HTTP_404_NOT_FOUND)
        except Exception as e:
            return Response({"data": "error", "details": str(e)}, status=status.HTTP_500_INTERNAL_SERVER_ERROR)
