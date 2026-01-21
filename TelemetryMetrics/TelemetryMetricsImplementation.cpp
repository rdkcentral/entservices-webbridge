/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2025 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <telemetry_busmessage_sender.h>
#include "TelemetryMetricsImplementation.h"
#include "UtilsLogging.h"
#include "TelemetryFilters.h"


namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(TelemetryMetricsImplementation, 1, 0);

    TelemetryMetricsImplementation::TelemetryMetricsImplementation()
    {
        LOGINFO("Create TelemetryMetricsImplementation Instance");
        t2_init((char *) "TelemetryMetrics");
    }

    TelemetryMetricsImplementation::~TelemetryMetricsImplementation()
    {
        t2_uninit();
        LOGINFO("Delete TelemetryMetricsImplementation Instance");
    }

    std::string generateRecordId(const std::string& id, const std::string& name)
    {
        if (id.empty() || name.empty())
        {
            LOGERR("Error: ID or Name is empty.");
            return "";
        }

        return id +":" +  name;
    }

/* This function attempts to parse a JSON-formatted string containing metrics,
 * validates the parsed data, and then merges it into an internal metrics record
 * map keyed by a generated record ID.
 *
 * @param id           The unique identifier
 * @param metrics      A JSON-formatted string representing the metrics data to record.
 * @param markerName   An string used to generate a unique record key.
 *
 * @return Core::hresult
 *         - Core::ERROR_NONE on success.
 *         - Core::ERROR_GENERAL if parsing fails or input is invalid.
 */
    Core::hresult TelemetryMetricsImplementation::Record(const std::string& id, const std::string& metrics, const std::string& markerName)
    {
        Core::hresult status = Core::ERROR_GENERAL;

        /* Create JSON parser builder and CharReader for parsing JSON strings */
        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

        Json::Value newMetrics = Json::objectValue;
        std::string errs = "";

        /*  Parse the input JSON string into newMetrics */
        if (!reader->parse(metrics.c_str(), metrics.c_str() + metrics.size(), &newMetrics, &errs))
        {
            LOGERR("JSON parse failed: %s", errs.c_str());
        }
        else if (!newMetrics.isObject())
        {
            LOGERR("Input metrics must be a JSON object");
        }
        else
        {
            std::lock_guard<std::mutex> lock(mMetricsMutex);
            std::string recordId = generateRecordId(id, markerName);
            if(!recordId.empty())
            {
                bool isNewRecord = (mMetricsRecord.find(recordId) == mMetricsRecord.end());

                /* Get existing metrics for the key or creates new entry if not exist */
                Json::Value& existing = mMetricsRecord[recordId];

                /* store markerName inside JSON value the first time */
                if (isNewRecord)
                {
                    existing["markerName"] = markerName;
                    LOGINFO("Storing new markerName '%s' for recordId '%s'", markerName.c_str(), recordId.c_str());
                }
                else
                {
                    LOGINFO("RecordId '%s' already exists. markerName unchanged.", recordId.c_str());
                }

                /* Merge each metric from newMetrics into existing record */
                for (const std::string& metricKey : newMetrics.getMemberNames())
                {
                    if (existing.isMember(metricKey))
                    {
                        LOGWARN("Record:'%s' Overwriting key '%s'", recordId.c_str(), metricKey.c_str());
                    }
                    else
                    {
                        LOGINFO("Record:'%s' Adding new key '%s'", recordId.c_str(), metricKey.c_str());
                    }
                    existing[metricKey] = newMetrics[metricKey];
                }
                status = Core::ERROR_NONE;
            }
        }

        return status;
    }

 /* Publishes the collected telemetry metrics.
 *
 * This method is responsible for sending or flushing the internally stored
 * metrics records to the telemetry.
 *
 * @param id           The unique identifier
 * @param markerName   An string used to generate a unique record key.
 *
 * @return Core::hresult
 *         - Core::ERROR_NONE on success.
 *         - Core::ERROR_GENERAL if parsing fails or input is invalid.
 */
    Core::hresult TelemetryMetricsImplementation::Publish(const std::string& id, const std::string& markerName)
    {
        Core::hresult status = Core::ERROR_GENERAL;

        std::string recordId = generateRecordId(id, markerName);
        Json::Value filteredMetrics =Json::objectValue;
        std::string appInstanceId = "";
        std::string matchedOtherRecordId = "";
        std::unordered_set<std::string> filterKeys ={};

        bool error = false;

        /* Lock mutex once for the entire critical section */
        std::lock_guard<std::mutex> lock(mMetricsMutex);

        /* Retrieve filter Names for the given markerName*/
        auto filterIt = markerFilters.find(markerName);
        if (filterIt != markerFilters.end())
        {
            filterKeys = filterIt->second;
        }
        else
        {
            LOGERR("Filter list not found for marker: %s", markerName.c_str());
            error = true;
        }

        if (!error)
        {
            /* Filter current recordMetrics and extract appInstanceId */
            auto currentRecordIt = mMetricsRecord.find(recordId);
            if (currentRecordIt == mMetricsRecord.end())
            {
                LOGERR("Current record not found: %s", recordId.c_str());
                error = true;
            }
            else
            {
                const Json::Value& currentMetrics = currentRecordIt->second;

                for (const std::string& key : currentMetrics.getMemberNames())
                {
                    if (filterKeys.count(key))
                    {
                        filteredMetrics[key] = currentMetrics[key];
                        if (key == "appInstanceId")
                        {
                            appInstanceId = currentMetrics[key].asString();
                        }
                    }
                    else 
                    {
                        LOGWARN("Key '%s' not allowed by filter for marker '%s'", key.c_str(), markerName.c_str());
                    }
                }
            }
        }

        /* Merge other recordMetrics with the same appInstanceId and markerName */
        if (!error && !appInstanceId.empty())
        {
            std::string appInstancePrefix = appInstanceId + ":";

            for (const auto& entry : mMetricsRecord)
            {
                const std::string& otherRecordId = entry.first;
                if (otherRecordId == recordId)
                    continue;

                if (otherRecordId.compare(0, appInstancePrefix.size(), appInstancePrefix) == 0)
                {
                    const std::string otherMarkerName = otherRecordId.substr(appInstancePrefix.size());

                    /* Merge if markerName Matches */
                    if (otherMarkerName == markerName)
                    {
                        const Json::Value& otherMetrics = entry.second;

                        for (const std::string& key : otherMetrics.getMemberNames())
                        {
                            if (filterKeys.count(key))
                            {
                                filteredMetrics[key] = otherMetrics[key];
                                LOGINFO("Merged key '%s' from '%s' into current record", key.c_str(), otherRecordId.c_str());
                            }
                        }
                        matchedOtherRecordId = otherRecordId;
                        LOGINFO("Merged record: '%s' into '%s'", otherRecordId.c_str(), recordId.c_str());
                        break;
                    }
                }
            }
        }

       /*Publish metrics if no errors occurred */
        if (!error)
        {
            Json::StreamWriterBuilder writerBuilder;
            writerBuilder["indentation"] = " ";
            std::string publishMetrics = Json::writeString(writerBuilder, filteredMetrics);

            LOGINFO("Publishing metrics for RecordId:'%s' publishMetrics:'%s'", recordId.c_str(), publishMetrics.c_str());
            t2_event_s((char*)markerName.c_str(), (char*)publishMetrics.c_str());

            /* Remove Published Record */
            mMetricsRecord.erase(recordId);
            if (!matchedOtherRecordId.empty())
            {
                mMetricsRecord.erase(matchedOtherRecordId);
            }
            LOGINFO("Cleared published record: %s", recordId.c_str());

            status = Core::ERROR_NONE;
        }

        return status;
    }
} /* namespace Plugin */
} /* namespace WPEFramework */
